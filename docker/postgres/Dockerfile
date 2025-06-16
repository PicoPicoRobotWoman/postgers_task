FROM postgres:15

RUN apt-get update && \
    apt-get install -y postgresql-server-dev-15 git make gcc && \
    git clone --branch v1.5.2 https://github.com/citusdata/pg_cron.git /tmp/pg_cron && \
    cd /tmp/pg_cron && \
    make && make install && \
    rm -rf /tmp/pg_cron && \
    apt-get remove --purge -y git make gcc && apt-get autoremove -y && apt-get clean

COPY log_user_changes_c.c /tmp/log_user_changes_c.c

RUN apt-get update && apt-get install -y gcc
RUN gcc -fPIC -c /tmp/log_user_changes_c.c \
    -I/usr/include/postgresql/15/server \
    -I/usr/include/postgresql/15/internal && \
    gcc -shared -o /usr/lib/postgresql/15/lib/log_user_changes_c.so log_user_changes_c.o && \
    rm /tmp/log_user_changes_c.c log_user_changes_c.o

