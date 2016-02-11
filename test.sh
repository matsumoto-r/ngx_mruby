#!/bin/bash

# Default install and test
#   download nginx into ./build/
#   build into ./build/nginx
#   test on ./build/nginx

set -e

. ./nginx_version

NGINX_INSTALL_DIR=`pwd`'/build/nginx'
NGINX_DEFUALT_OPT="--with-http_stub_status_module --with-http_ssl_module --with-cc-opt='-Wno-error'"

if [ $NGINX_SRC_MINOR -ge 9 ]; then
  if [ $NGINX_SRC_PATCH -ge 6 ]; then
    NGINX_CONFIG_OPT="--prefix=${NGINX_INSTALL_DIR} ${NGINX_DEFUALT_OPT} --with-stream --without-stream_access_module"
  else
  NGINX_CONFIG_OPT="--prefix=${NGINX_INSTALL_DIR} ${NGINX_DEFUALT_OPT}"
  fi
else
  NGINX_CONFIG_OPT="--prefix=${NGINX_INSTALL_DIR} ${NGINX_DEFUALT_OPT}"
fi

if [ "$NUM_THREADS_ENV" != "" ]; then
    NUM_THREADS=$NUM_THREADS_ENV
else
    NUM_PROCESSORS=`getconf _NPROCESSORS_ONLN`
    if [ $NUM_PROCESSORS -gt 1 ]; then
        NUM_THREADS=$(expr $NUM_PROCESSORS / 2)
    else
        NUM_THREADS=1
    fi
fi

echo "NGINX_CONFIG_OPT=$NGINX_CONFIG_OPT"
echo "NUM_THREADS=$NUM_THREADS"

if [ ! -d "./mruby/src" ]; then
    echo "mruby Downloading ..."
    git submodule init
    git submodule update
    echo "mruby Downloading ... Done"
fi

if [ "$ONLY_BUILD_NGX_MRUBY" = "" ]; then

  echo "nginx Downloading ..."
  if [ -d "./build" ]; then
      echo "build directory was found"
  else
      mkdir build
  fi
  cd build
  if [ ! -e ${NGINX_SRC_VER} ]; then
      wget http://nginx.org/download/${NGINX_SRC_VER}.tar.gz
      echo "nginx Downloading ... Done"
      tar xf ${NGINX_SRC_VER}.tar.gz
  fi
  ln -sf ${NGINX_SRC_VER} nginx_src
  NGINX_SRC=`pwd`'/nginx_src'
  cd ..

  echo "ngx_mruby configure ..."
  ./configure --with-ngx-src-root=${NGINX_SRC} --with-ngx-config-opt="${NGINX_CONFIG_OPT}"
  echo "ngx_mruby configure ... Done"

  echo "mruby building ..."
  make build_mruby NUM_THREADS=$NUM_THREADS -j $NUM_THREADS
  echo "mruby building ... Done"

  echo "ngx_mruby building ..."
  make NUM_THREADS=$NUM_THREADS -j $NUM_THREADS
else
  make make_ngx_mruby NUM_THREADS=$NUM_THREADS -j $NUM_THREADS
fi

echo "ngx_mruby building ... Done"

echo "ngx_mruby testing ..."
make install
ps -C nginx && killall nginx
sed -e "s|__NGXDOCROOT__|${NGINX_INSTALL_DIR}/html/|g" test/conf/nginx.conf > ${NGINX_INSTALL_DIR}/conf/nginx.conf
cd ${NGINX_INSTALL_DIR}/html && sh -c 'yes "" | openssl req -new -days 365 -x509 -nodes -keyout localhost.key -out localhost.crt' && sh -c 'yes "" | openssl req -new -days 1 -x509 -nodes -keyout dummy.key -out dummy.crt' && cd -

if [ $NGINX_SRC_MINOR -ge 9 ]; then
  if [ $NGINX_SRC_PATCH -ge 6 ]; then
    cat test/conf/nginx.stream.conf >> ${NGINX_INSTALL_DIR}/conf/nginx.conf
  fi
fi

cp -pr test/html/* ${NGINX_INSTALL_DIR}/html/.

echo "====================================="
echo ""
echo "ngx_mruby starting and logging"
echo ""
echo "====================================="
echo ""
echo ""
${NGINX_INSTALL_DIR}/sbin/nginx &
echo ""
echo ""
cp -p test/build_config.rb ./mruby_test/.
cd mruby_test
rake
./bin/mruby ../test/t/ngx_mruby.rb
killall nginx
echo "ngx_mruby testing ... Done"

echo "test.sh ... successful"
