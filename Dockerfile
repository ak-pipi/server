# ============================================================
# Stage 1: Build the C++ game server
# ============================================================
FROM alibaba-cloud-linux-3-registry.cn-chengdu.cr.aliyuncs.com/alinux3/alinux3 AS builder

# Enable CRB repository for boost-static
RUN dnf install -y dnf-plugins-core && \
    (dnf config-manager --set-enabled PowerTools || dnf config-manager --set-enabled crb || true)

# Install build dependencies
RUN dnf install -y \
        gcc \
        gcc-c++ \
        cmake \
        make \
        git \
        openssl-devel \
        boost-devel \
        boost-static \
        mysql-devel \
    && dnf clean all

# Build rabbitmq-c (static library)
ARG RABBITMQ_C_VERSION=v0.15.0
ARG BUILD_JOBS=2
RUN cd /tmp && \
    git clone --depth 1 --branch ${RABBITMQ_C_VERSION} https://github.com/alanxz/rabbitmq-c.git && \
    cd rabbitmq-c && \
    mkdir build && cd build && \
    cmake .. \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DENABLE_SSL_SUPPORT=ON \
        -DBUILD_STATIC_LIBS=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_BUILD_TYPE=Release && \
    make -j${BUILD_JOBS} && \
    make install && \
    rm -rf /tmp/rabbitmq-c

# Build MySQL Connector/C++ 9.x
ARG MYSQL_CONCPP_VERSION=9.2.0
RUN cd /tmp && \
    git clone --depth 1 --branch ${MYSQL_CONCPP_VERSION} https://github.com/mysql/mysql-connector-cpp.git && \
    cd mysql-connector-cpp && \
    mkdir build && cd build && \
    cmake .. \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DWITH_SSL=system \
        -DWITH_JDBC=ON \
        -DCMAKE_BUILD_TYPE=Release && \
    make -j${BUILD_JOBS} && \
    make install && \
    rm -rf /tmp/mysql-connector-cpp

# Build the game server
WORKDIR /build
COPY . /build
RUN mkdir -p build_output && \
    cd build_output && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j${BUILD_JOBS}

# ============================================================
# Stage 2: Runtime image
# ============================================================
FROM alibaba-cloud-linux-3-registry.cn-chengdu.cr.aliyuncs.com/alinux3/alinux3

# Install runtime dependencies
RUN dnf install -y dnf-plugins-core && \
    (dnf config-manager --set-enabled PowerTools || dnf config-manager --set-enabled crb || true) && \
    dnf install -y \
        openssl-libs \
        boost-runtime \
    && dnf clean all

WORKDIR /app
COPY --from=builder /build/build_output/Server/Server /app/Server
COPY --from=builder /usr/local/lib/libmysqlcppconn* /usr/local/lib/
RUN ldconfig
RUN mkdir -p /app/log

EXPOSE 10086 9098

# server.ini is mounted by deploy/aliyun-deploy.sh so credentials never enter the image.
CMD ["./Server"]
