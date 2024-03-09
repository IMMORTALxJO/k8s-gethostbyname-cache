FROM rust:1.76.0-buster as builder
COPY . /build
RUN apt-get update && apt-get -y install build-essential zlib1g-dev \
		llvm-12-dev libclang-12-dev linux-headers-generic \
		libelf-dev && \
    apt-get clean
RUN cd /build && cargo build


FROM php:7.4-fpm
COPY --from=builder  /build/xdp_dns_cache.o /