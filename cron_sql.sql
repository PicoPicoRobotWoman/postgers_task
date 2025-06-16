CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name TEXT,
    email TEXT,
    role TEXT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);


CREATE TABLE users_audit (
    id SERIAL PRIMARY KEY,
    user_id INTEGER,
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    changed_by TEXT,
    field_changed TEXT,
    old_value TEXT,
    new_value TEXT
);

CREATE OR REPLACE FUNCTION log_user_changes()
RETURNS trigger
AS 'log_user_changes_c', 'log_user_changes_c'
LANGUAGE C STRICT;

CREATE OR REPLACE TRIGGER trg_user_update_audit
AFTER UPDATE ON users
FOR EACH ROW
EXECUTE FUNCTION log_user_changes();


CREATE EXTENSION pg_cron;


CREATE OR REPLACE FUNCTION export_today_users_audit()
RETURNS void AS $$
DECLARE
    file_path TEXT := '/tmp/users_audit_export_' || to_char(CURRENT_DATE, 'YYYY-MM-DD') || '.csv';
BEGIN
    EXECUTE format(
        'COPY (SELECT * FROM users_audit WHERE changed_at::date = CURRENT_DATE) TO %L WITH CSV HEADER',
        file_path
    );
END;
$$ LANGUAGE plpgsql;

SELECT cron.schedule(
    'daily_audit_export',
    '0 3 * * *',
    $$ SELECT export_today_users_audit(); $$
);

SELECT * FROM cron.job;
