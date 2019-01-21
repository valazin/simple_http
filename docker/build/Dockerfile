#docker build -t registry.avalab.io/videos/processing/videos.transform/storage-build .

FROM registry.avalab.io/videos/processing/videos.transform/storage-production-environment

WORKDIR /opt

COPY simple_http .

ENTRYPOINT ["/opt/simple_http"]
