// Copyright 2010-2014 RethinkDB, all rights reserved.
#include "clustering/administration/artificial_reql_cluster_interface.hpp"

#include "clustering/administration/main/watchable_fields.hpp"
#include "clustering/administration/metadata.hpp"
#include "clustering/administration/real_reql_cluster_interface.hpp"
#include "rdb_protocol/artificial_table/artificial_table.hpp"
#include "rpc/semilattice/view/field.hpp"

bool artificial_reql_cluster_interface_t::db_create(const name_string_t &name,
            signal_t *interruptor, std::string *error_out) {
    if (name == database) {
        *error_out = strprintf("Database `%s` already exists.", database.c_str());
        return false;
    }
    return next->db_create(name, interruptor, error_out);
}

bool artificial_reql_cluster_interface_t::db_drop(const name_string_t &name,
        signal_t *interruptor, std::string *error_out) {
    if (name == database) {
        *error_out = strprintf("Database `%s` is special; you can't delete it.",
            database.c_str());
        return false;
    }
    return next->db_drop(name, interruptor, error_out);
}

bool artificial_reql_cluster_interface_t::db_list(
        signal_t *interruptor,
        std::set<name_string_t> *names_out, std::string *error_out) {
    if (!next->db_list(interruptor, names_out, error_out)) {
        return false;
    }
    guarantee(names_out->count(database) == 0);
    names_out->insert(database);
    return true;
}

bool artificial_reql_cluster_interface_t::db_find(const name_string_t &name,
        signal_t *interruptor,
        counted_t<const ql::db_t> *db_out, std::string *error_out) {
    if (name == database) {
        *db_out = make_counted<const ql::db_t>(nil_uuid(), database.str());
        return true;
    }
    return next->db_find(name, interruptor, db_out, error_out);
}

bool artificial_reql_cluster_interface_t::table_create(const name_string_t &name,
        counted_t<const ql::db_t> db, const boost::optional<name_string_t> &primary_dc,
        bool hard_durability, const std::string &primary_key, signal_t *interruptor,
        std::string *error_out) {
    if (db->name == database.str()) {
        *error_out = strprintf("Database `%s` is special; you can't create new tables "
            "in it.", database.c_str());
        return false;
    }
    return next->table_create(name, db, primary_dc, hard_durability, primary_key,
        interruptor, error_out);
}

bool artificial_reql_cluster_interface_t::table_drop(const name_string_t &name,
        counted_t<const ql::db_t> db, signal_t *interruptor, std::string *error_out) {
    if (db->name == database.str()) {
        *error_out = strprintf("Database `%s` is special; you can't drop tables in it.",
            database.c_str());
        return false;
    }
    return next->table_drop(name, db, interruptor, error_out);
}

bool artificial_reql_cluster_interface_t::table_list(counted_t<const ql::db_t> db,
        signal_t *interruptor,
        std::set<name_string_t> *names_out, std::string *error_out) {
    if (db->name == database.str()) {
        for (auto it = tables.begin(); it != tables.end(); ++it) {
            if (it->first.str()[0] == '_') {
                /* If a table's name starts with `_`, don't show it to the user unless
                they explicitly request it. */
                continue;
            }
            names_out->insert(it->first);
        }
        return true;
    }
    return next->table_list(db, interruptor, names_out, error_out);
}

bool artificial_reql_cluster_interface_t::table_find(const name_string_t &name,
        counted_t<const ql::db_t> db, signal_t *interruptor,
        scoped_ptr_t<base_table_t> *table_out, std::string *error_out) {
    if (db->name == database.str()) {
        auto it = tables.find(name);
        if (it != tables.end()) {
            table_out->init(new artificial_table_t(it->second));
            return true;
        } else {
            *error_out = strprintf("Table `%s.%s` does not exist.",
                database.c_str(), name.c_str());
            return false;
        }
    }
    return next->table_find(name, db, interruptor, table_out, error_out);
}

bool artificial_reql_cluster_interface_t::table_config(
        counted_t<const ql::db_t> db,
        const std::vector<name_string_t> &target_tables,
        const ql::protob_t<const Backtrace> &bt, signal_t *interruptor,
        scoped_ptr_t<ql::val_t> *resp_out, std::string *error_out) {
    if (db->name == database.str()) {
        *error_out = strprintf("Database `%s` is special; you can't configure the "
            "tables in it.", database.c_str());
        return false;
    }
    return next->table_config(db, target_tables, bt, interruptor, resp_out, error_out);
}

bool artificial_reql_cluster_interface_t::table_status(
        counted_t<const ql::db_t> db,
        const std::vector<name_string_t> &target_tables,
        const ql::protob_t<const Backtrace> &bt, signal_t *interruptor,
        scoped_ptr_t<ql::val_t> *resp_out, std::string *error_out) {
    if (db->name == database.str()) {
        *error_out = strprintf("Database `%s` is special; the system tables in it don't "
            "have meaningful status information.", database.c_str());
        return false;
    }
    return next->table_status(db, target_tables, bt, interruptor, resp_out, error_out);
}

bool artificial_reql_cluster_interface_t::table_wait(
        counted_t<const ql::db_t> db,
        const std::vector<name_string_t> &target_tables,
        table_readiness_t readiness,
        const ql::protob_t<const Backtrace> &bt, signal_t *interruptor,
        scoped_ptr_t<ql::val_t> *resp_out, std::string *error_out) {
    if (db->name == database.str()) {
        *error_out = strprintf("Database `%s` is special; the system tables in it are "
            "always available and don't need to be waited on.", database.c_str());
        return false;
    }
    return next->table_wait(db, target_tables, readiness,
                            bt, interruptor, resp_out, error_out);
}

bool artificial_reql_cluster_interface_t::table_reconfigure(
        counted_t<const ql::db_t> db,
        const name_string_t &name,
        const table_generate_config_params_t &params,
        bool dry_run,
        signal_t *interruptor,
        ql::datum_t *new_config_out,
        std::string *error_out) {
    if (db->name == database.str()) {
        *error_out = strprintf("Database `%s` is special; you can't configure the "
            "tables in it.", database.c_str());
        return false;
    }
    return next->table_reconfigure(db, name, params, dry_run, interruptor,
        new_config_out, error_out);
}

bool artificial_reql_cluster_interface_t::db_reconfigure(
        counted_t<const ql::db_t> db,
        const table_generate_config_params_t &params,
        bool dry_run,
        signal_t *interruptor,
        ql::datum_t *new_config_out,
        std::string *error_out) {
    if (db->name == database.str()) {
        *error_out = strprintf("Database `%s` is special; you can't configure the "
            "tables in it.", database.c_str());
        return false;
    }
    return next->db_reconfigure(db, params, dry_run, interruptor,
        new_config_out, error_out);
}

bool artificial_reql_cluster_interface_t::table_estimate_doc_counts(
        counted_t<const ql::db_t> db,
        const name_string_t &name,
        ql::env_t *env,
        std::vector<int64_t> *doc_counts_out,
        std::string *error_out) {
    if (db->name == database.str()) {
        auto it = tables.find(name);
        if (it != tables.end()) {
            counted_t<ql::datum_stream_t> docs;
            if (!it->second->read_all_rows_as_stream(
                    ql::protob_t<const Backtrace>(),
                    datum_range_t::universe(),
                    sorting_t::UNORDERED,
                    env->interruptor,
                    &docs,
                    error_out)) {
                *error_out = "When estimating doc count: " + *error_out;
                return false;
            }
            try {
                scoped_ptr_t<ql::val_t> count =
                    docs->run_terminal(env, ql::count_wire_func_t());
                *doc_counts_out = std::vector<int64_t>({ count->as_int<int64_t>() });
            } catch (const ql::base_exc_t &msg) {
                *error_out = "When estimating doc count: " + std::string(msg.what());
                return false;
            }
            return true;
        } else {
            *error_out = strprintf("Table `%s.%s` does not exist.",
                database.c_str(), name.c_str());
            return false;
        }
    } else {
        return next->table_estimate_doc_counts(db, name, env, doc_counts_out, error_out);
    }
}

admin_artificial_tables_t::admin_artificial_tables_t(
        real_reql_cluster_interface_t *_next_reql_cluster_interface,
        boost::shared_ptr< semilattice_readwrite_view_t<
            cluster_semilattice_metadata_t> > _semilattice_view,
        boost::shared_ptr< semilattice_readwrite_view_t<
            auth_semilattice_metadata_t> > _auth_view,
        clone_ptr_t< watchable_t< change_tracking_map_t<peer_id_t,
            cluster_directory_metadata_t> > > _directory_view,
        watchable_map_t<std::pair<peer_id_t, namespace_id_t>,
                            namespace_directory_metadata_t> *_reactor_directory_view,
        server_name_client_t *_name_client) {
    std::map<name_string_t, artificial_table_backend_t*> backends;

    cluster_config_backend.init(new cluster_config_artificial_table_backend_t(
        _auth_view));
    backends[name_string_t::guarantee_valid("cluster_config")] =
        cluster_config_backend.get();

    db_config_backend.init(new db_config_artificial_table_backend_t(
        metadata_field(&cluster_semilattice_metadata_t::databases,
            _semilattice_view)));
    backends[name_string_t::guarantee_valid("db_config")] =
        db_config_backend.get();

    issues_backend.init(new issues_artificial_table_backend_t(
        _semilattice_view, _directory_view));
    backends[name_string_t::guarantee_valid("issues")] =
        issues_backend.get();

    server_config_backend.init(new server_config_artificial_table_backend_t(
        metadata_field(&cluster_semilattice_metadata_t::servers,
            _semilattice_view),
        _name_client));
    backends[name_string_t::guarantee_valid("server_config")] =
        server_config_backend.get();

    server_status_backend.init(new server_status_artificial_table_backend_t(
        metadata_field(&cluster_semilattice_metadata_t::servers,
            _semilattice_view),
        _name_client,
        _directory_view,
        metadata_field(&cluster_semilattice_metadata_t::rdb_namespaces,
            _semilattice_view),
        metadata_field(&cluster_semilattice_metadata_t::databases,
            _semilattice_view)));
    backends[name_string_t::guarantee_valid("server_status")] =
        server_status_backend.get();

    table_config_backend.init(new table_config_artificial_table_backend_t(
        _semilattice_view,
        _next_reql_cluster_interface,
        _name_client));
    backends[name_string_t::guarantee_valid("table_config")] =
        table_config_backend.get();

    table_status_backend.init(new table_status_artificial_table_backend_t(
        _semilattice_view,
        _reactor_directory_view,
        _name_client));
    backends[name_string_t::guarantee_valid("table_status")] =
        table_status_backend.get();

    debug_scratch_backend.init(new in_memory_artificial_table_backend_t);
    backends[name_string_t::guarantee_valid("_debug_scratch")] =
        debug_scratch_backend.get();

    debug_table_status_backend.init(new debug_table_status_artificial_table_backend_t(
        _semilattice_view,
        _reactor_directory_view,
        _name_client));
    backends[name_string_t::guarantee_valid("_debug_table_status")] =
        debug_table_status_backend.get();

    reql_cluster_interface.init(new artificial_reql_cluster_interface_t(
        name_string_t::guarantee_valid("rethinkdb"),
        backends,
        _next_reql_cluster_interface));
}
