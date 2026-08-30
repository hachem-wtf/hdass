FROM --platform=linux/amd64 debian:trixie-slim

ARG PREMAKE_VERSION=5.0.0-beta8

RUN apt-get update && apt-get install -y --no-install-recommends \
	build-essential \
	nasm \
	binutils \
	make \
	curl \
	ca-certificates \
	&& rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "https://github.com/premake/premake-core/releases/download/v${PREMAKE_VERSION}/premake-${PREMAKE_VERSION}-linux.tar.gz" \
	| tar -xz -C /usr/local/bin \
	&& chmod +x /usr/local/bin/premake5

WORKDIR /hdass
