#docker build -t registry.avalab.io/videos/processing/videos.transform/storage-production-environment .

FROM registry.avalab.io/faceter/faceter.recognition.core/mongo-cxx-driver as MONGO_CXX_DRIVER_IMAGE
FROM ubuntu:18.04

ENV ROOT_PATH="/opt"

# Prepare dependencies
# from repository
ENV REQUIREMENTS="libconfig++9v5 \
                  libgoogle-glog0v5 \
                  libgflags2.2 \
                  build-essential"
ENV REQUIREMENTS_TYPE="prod"

# from other images
COPY --from=MONGO_CXX_DRIVER_IMAGE $ROOT_PATH/$REQUIREMENTS_TYPE $ROOT_PATH/$REQUIREMENTS_TYPE

RUN apt update && apt install -y --no-install-recommends $(ls $ROOT_PATH/$REQUIREMENTS_TYPE/*.deb) $(echo $REQUIREMENTS) && \
    rm -rf /var/lib/atp/lists/* && rm -rf /opt/$REQUIREMENTS_TYPE

