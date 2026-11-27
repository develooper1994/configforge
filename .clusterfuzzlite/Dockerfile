# ClusterFuzzLite's builder: the OSS-Fuzz base image, with this
# repository at $SRC/configforge and build.sh beside it. The image is pinned by
# digest (OpenSSF Scorecard, PinnedDependencies); `latest` of 2026-11-24.
FROM gcr.io/oss-fuzz-base/base-builder@sha256:8954f7ece3f9e0731a748ffd91ee966bc032b3340725e773aef3d7e82eb5d8b6
RUN apt-get update && apt-get install -y --no-install-recommends cmake
COPY . $SRC/configforge
WORKDIR $SRC/configforge
COPY .clusterfuzzlite/build.sh $SRC/
