# ClusterFuzzLite's builder: the OSS-Fuzz base image, with this
# repository at $SRC/configforge and build.sh beside it.
FROM gcr.io/oss-fuzz-base/base-builder
RUN apt-get update && apt-get install -y --no-install-recommends cmake
COPY . $SRC/configforge
WORKDIR $SRC/configforge
COPY .clusterfuzzlite/build.sh $SRC/
