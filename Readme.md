
=======================================================================================================================
installation guide grpc
https://grpc.io/docs/languages/cpp/quickstart/



define ohlc.proto

ohlc.proto


generate c++ files first by below command

protoc -I=. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ohlc.proto                                                                       
protoc -I=. --cpp_out=. ohlc.proto  








=======================================================================================================================
Redis setup guide
brew install hiredis
brew install redis



//hiredis install
git clone https://github.com/redis/hiredis.git

cd hiredis

make

sudo make install




//redis-plus-plus install
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build
cd build
cmake ..
make
sudo make install







=======================================================================================================================
//jsoncpp install
git clone https://github.com/open-source-parsers/jsoncpp.git
cd jsoncpp
mkdir build
cd build
cmake ..
make
sudo make install



//include
#include <json/json.h>


//compile and run

g++ test.cpp -o test -std=c++11 -ljsoncpp

./test





 



=====================compile and run =============================================================


//compile server.cpp
g++ -std=c++17 -o server server.cpp ohlc.grpc.pb.cc ohlc.pb.cc -pthread \
    `pkg-config --cflags protobuf grpc++ grpc` `pkg-config --libs protobuf grpc++ grpc` \
    -lgrpc++_reflection -ljsoncpp -lhiredis





//compile producer.cpp
g++ -std=c++17 -o producer producer.cpp ohlc.grpc.pb.cc ohlc.pb.cc -pthread \
    `pkg-config --cflags protobuf grpc++ grpc` `pkg-config --libs protobuf grpc++ grpc` \
    -lgrpc++_reflection -ljsoncpp





//compile client.cpp
g++ -std=c++17 -o client client.cpp ohlc.grpc.pb.cc ohlc.pb.cc -pthread \
    `pkg-config --cflags protobuf grpc++ grpc` `pkg-config --libs protobuf grpc++ grpc` \
    -lgrpc++_reflection -ljsoncpp -lhiredis



//RUN BELOW THESE IN THREE DIFFERENT TERMINALS


redis-server



//run server->STORING IN REDIS OHLC VALUES RECEIVED
./server 50051



//run producer-->SEND THE OHLC VALUES TO SERVER 
./producer


//run client to test any stock code data
./client  UNVR  //IT WILL GIVE OHLC VALUES OF UNVR ,, YOU CAN CHANGE TO ANY OTHER STOCK CODE ALSO

