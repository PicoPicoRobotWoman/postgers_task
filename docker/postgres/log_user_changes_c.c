#include "postgres.h"
#include "fmgr.h"
#include "executor/spi.h"
#include "commands/trigger.h"
#include "utils/rel.h"
#include "utils/builtins.h"
#include "miscadmin.h"
#include "utils/syscache.h"
#include "catalog/pg_authid.h"
#include "utils/lsyscache.h"
#include "utils/acl.h"
#include "catalog/pg_collation.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(log_user_changes_c);

static void
log_change_if_needed(HeapTuple oldtup, HeapTuple newtup, TupleDesc tupdesc,
                     const char *fieldname, int attnum, const char *current_user_name)
{
    bool isnull_old, isnull_new;
    Datum old_val = heap_getattr(oldtup, attnum, tupdesc, &isnull_old);
    Datum new_val = heap_getattr(newtup, attnum, tupdesc, &isnull_new);

    if (isnull_old || isnull_new)
        return;

    // Convert text Datum to C-string
    char *old_str = TextDatumGetCString(old_val);
    char *new_str = TextDatumGetCString(new_val);

    if (strcmp(old_str, new_str) != 0)
    {
        Datum user_id_datum = heap_getattr(oldtup, 1, tupdesc, &isnull_old);
        if (isnull_old)
            elog(ERROR, "user_id is null");

        int32 user_id = DatumGetInt32(user_id_datum);

        const char *cmd = "INSERT INTO users_audit(user_id, changed_by, field_changed, old_value, new_value) "
                          "VALUES ($1, $2, $3, $4, $5)";

        Oid argtypes[5] = {INT4OID, TEXTOID, TEXTOID, TEXTOID, TEXTOID};
        Datum values[5];
        char nulls[5] = {' ', ' ', ' ', ' ', ' '};

        values[0] = Int32GetDatum(user_id);
        values[1] = CStringGetTextDatum(current_user_name);
        values[2] = CStringGetTextDatum(fieldname);
        values[3] = CStringGetTextDatum(old_str);
        values[4] = CStringGetTextDatum(new_str);

        int ret = SPI_execute_with_args(cmd, 5, argtypes, values, nulls, false, 0);
        if (ret != SPI_OK_INSERT)
            elog(ERROR, "SPI_exec failed for %s insert", fieldname);
    }
}


Datum
log_user_changes_c(PG_FUNCTION_ARGS)
{
    TriggerData *trigdata;
    HeapTuple newtup, oldtup;
    TupleDesc tupdesc;
    char *current_user_name;

    if (!CALLED_AS_TRIGGER(fcinfo))
        elog(ERROR, "function not called by trigger manager");

    trigdata = (TriggerData *) fcinfo->context;

    if (!TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
        return PointerGetDatum(trigdata->tg_newtuple);

    newtup = trigdata->tg_newtuple;
    oldtup = trigdata->tg_trigtuple;
    tupdesc = trigdata->tg_relation->rd_att;

    current_user_name = GetUserNameFromId(GetUserId(), false);

    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "SPI_connect failed");

    log_change_if_needed(oldtup, newtup, tupdesc, "name", 2, current_user_name);
    log_change_if_needed(oldtup, newtup, tupdesc, "email", 3, current_user_name);
    log_change_if_needed(oldtup, newtup, tupdesc, "role", 4, current_user_name);

    SPI_finish();

    return PointerGetDatum(newtup);
}
