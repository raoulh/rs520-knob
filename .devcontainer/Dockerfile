FROM espressif/idf:release-v6.0

ARG DEBIAN_FRONTEND=nointeractive
ARG CONTAINER_USER=esp
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN curl -fsSL https://deb.nodesource.com/setup_22.x | bash -

RUN apt-get update \
  && apt install -y -q \
  cmake iproute2 \
  git \
  hwdata \
  libglib2.0-0 \
  libnuma1 \
  libpixman-1-0 \
  linux-tools-virtual \
  sudo nodejs \
  build-essential \
  libzstd-dev \
  libmbedtls-dev \
  libcriterion-dev \
  meson gdb gdbserver \
  golang \
  cifs-utils \
  python3-pip \
  python3-png \
  python3-lz4 \
  libx11-dev \
  libsdl2-dev \
  libwebsockets-dev \
  && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --break-system-packages pypng
RUN python3 -m pip install --break-system-packages lz4

RUN sudo npm i lv_i18n -g
RUN sudo npm i lv_font_conv -g

RUN update-alternatives --install /usr/local/bin/usbip usbip `ls /usr/lib/linux-tools/*/usbip | tail -n1` 20

# QEMU
ENV QEMU_REL=esp-develop-20220919
ENV QEMU_SHA256=f6565d3f0d1e463a63a7f81aec94cce62df662bd42fc7606de4b4418ed55f870
ENV QEMU_DIST=qemu-${QEMU_REL}.tar.bz2
ENV QEMU_URL=https://github.com/espressif/qemu/releases/download/${QEMU_REL}/${QEMU_DIST}

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

RUN wget --no-verbose ${QEMU_URL} \
  && echo "${QEMU_SHA256} *${QEMU_DIST}" | sha256sum --check --strict - \
  && tar -xf $QEMU_DIST -C /opt \
  && rm ${QEMU_DIST}

RUN mkdir -p /root/.ssh \
  && chmod 700 /root/.ssh \
  && touch /root/.ssh/known_hosts \
  && ssh-keyscan github.com >> /root/.ssh/known_hosts

ENV PATH=/opt/qemu/bin:${PATH}

#better bash history search with up/down arrows
RUN echo "\n\
## arrow up \n\
\"\e[A\":history-search-backward \n\
## arrow down \n\
\"\e[B\":history-search-forward \n\
\n\
set show-all-if-ambiguous on \n\
set completion-ignore-case on \n\
" >> /etc/inputrc

RUN curl -fsSL https://starship.rs/install.sh | sh -s -- -y

RUN chmod -R 775 /opt/esp/python_env/

RUN echo "esp ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

RUN usermod -u 1001 ubuntu && groupmod -g 1001 ubuntu && \
    groupadd --gid $USER_GID $CONTAINER_USER && \
    adduser --uid $USER_UID --gid $USER_GID --disabled-password --gecos "" ${CONTAINER_USER} && \
    usermod -a -G dialout $CONTAINER_USER && \
    usermod -a -G sudo $CONTAINER_USER && \
    usermod -a -G root $CONTAINER_USER && \
    echo "esp:esp" | chpasswd

USER ${CONTAINER_USER}
ENV USER=${CONTAINER_USER}
WORKDIR /home/${CONTAINER_USER}

RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> ~/.bashrc

RUN echo 'eval "$(starship init bash)"' >> ~/.bashrc

RUN IDF_PATH_FORCE=1 . /opt/esp/idf/export.sh IDF_PATH=/opt/esp/idf && \
    pip3 install pypng lz4

ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]

CMD ["/bin/bash"]

