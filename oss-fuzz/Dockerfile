# OSS-Fuzz's builder: the base image, with the repository cloned to
# $SRC/configforge and build.sh beside it.
FROM gcr.io/oss-fuzz-base/base-builder
RUN apt-get update && apt-get install -y cmake
RUN git clone --depth 1 https://github.com/develooper1994/configforge $SRC/configforge
WORKDIR $SRC/configforge
COPY build.sh $SRC/
