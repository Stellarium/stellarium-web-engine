FROM ubuntu:18.04

RUN apt-get update && apt-get install -y wget build-essential cmake python apt-transport-https git scons pkg-config nodejs

# Download emscripten SDK
RUN wget https://s3.amazonaws.com/mozilla-games/emscripten/releases/emsdk-portable.tar.gz
RUN tar -xzf emsdk-portable.tar.gz
WORKDIR /emsdk-portable

# Fetch the latest registry of available tools.
RUN ./emsdk update

# Download and install the latest SDK tools.
RUN ./emsdk install sdk-1.38.11-64bit

# Set up the compiler configuration to point to the "latest" SDK.
RUN ./emsdk activate sdk-1.38.11-64bit

EXPOSE 8000

WORKDIR /app
CMD /bin/bash -c "source /emsdk-portable/emsdk_env.sh && make js" && cd html && python -m SimpleHTTPServer

