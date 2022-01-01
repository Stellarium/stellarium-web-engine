FROM python:3
ENV EMSCRIPTEN_VERSION=2.0.34
RUN pip3 install scons
WORKDIR /emscripten
RUN curl -L https://github.com/emscripten-core/emsdk/archive/refs/tags/${EMSCRIPTEN_VERSION}.tar.gz | tar --strip-components=1 -xvzf - && \
    ./emsdk install ${EMSCRIPTEN_VERSION} && \
    ./emsdk activate ${EMSCRIPTEN_VERSION}
WORKDIR /build
COPY . /build
RUN cd /emscripten && . /emscripten/emsdk_env.sh && cd /build && make js

FROM nginx:alpine
COPY --from=0 /build/apps/ /usr/share/nginx/html/
COPY --from=0 /build/build/ /usr/share/nginx/html/build/
