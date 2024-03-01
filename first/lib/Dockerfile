FROM gcc:9.5.0-bullseye as builder
COPY . /build
RUN cd /build && gcc -shared -fPIC -o dns-cache.so main.c -ldl

FROM php:7.4-fpm
COPY --from=builder  /build/dns-cache.so /