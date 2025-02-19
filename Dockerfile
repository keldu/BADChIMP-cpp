FROM docker.io/nixos/nix:latest

# Sadly a hack for the internal TLS MITM setup
ENV NIX_SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
ENV SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt

# Guarantee that /data exists
RUN mkdir -p /code /data
WORKDIR /data
COPY . /code

# guarantee that the image has the dev files prebuilt
RUN nix-build /code

RUN mkdir -p /data/input/mpi /data/output

# Rather mount /etc/ssl/certs yourself when running the Dockerfile with -v /etc/ssl/certs:/etc/ssl/certs

ENTRYPOINT ["nix-shell", "/code/default.nix"]
