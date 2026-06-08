FROM gcr.io/oss-fuzz-base/base-builder
RUN apt-get update && apt-get install -y make cmake
COPY . $SRC/cjsonx
WORKDIR $SRC/cjsonx
COPY .clusterfuzzlite/build.sh $SRC/
