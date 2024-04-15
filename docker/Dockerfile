FROM debian:bookworm

RUN apt-get update -qq && \
    apt-get install -y -qq build-essential gcovr autoconf pkg-config && \
    apt-get install -y -qq libpoppler-cpp-dev libgcrypt20-dev libpcre2-dev asciidoc dejagnu texlive-latex-base

ENV CXXFLAGS "-Wformat -Werror=format-security -Wp,-D_FORTIFY_SOURCE=2 -Wp,-D_GLIBCXX_ASSERTIONS -fexceptions -fstack-protector-strong -grecord-gcc-switches"

WORKDIR /pdfgrep

CMD ./autogen.sh && ./configure --enable-coverage && make check && gcovr -r .
