# ClusterFuzzLite's builder: the OSS-Fuzz base image, with this
# repository at $SRC/configforge and build.sh beside it. The image is pinned by
# digest (OpenSSF Scorecard, PinnedDependencies); `latest` of 2026-11-24.
FROM gcr.io/oss-fuzz-base/base-builder@sha256:46319ce66bf966bda1f47fdfcfb64b896e0311d3d2243bffb3a8f8b0916c1338
RUN apt-get update && apt-get install -y --no-install-recommends cmake
COPY . $SRC/configforge
WORKDIR $SRC/configforge
COPY .clusterfuzzlite/build.sh $SRC/
