#include <grpc++/grpc++.h>
#include "ohlc.grpc.pb.h"
#include <hiredis/hiredis.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <memory>

template <typename ExceptionType>
class ExceptionHandler {
public:
    template <typename Func>
    static void Handle(Func func, const std::string& errorMessage = "An error occurred.") {
        try {
            func();
        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            throw ExceptionType(errorMessage);
        } catch (...) {
            std::cerr << "Unknown exception occurred." << std::endl;
            throw ExceptionType(errorMessage);
        }
    }
};

struct OHLCData {
    double open;
    double high;
    double low;
    double close;
    int volume;
    double value;
    std::string stockCode;
};

class OHLCWithRedisException : public std::runtime_error {
public:
    OHLCWithRedisException(const std::string& message) : std::runtime_error(message) {}
};

class RedisConnection {
public:
    RedisConnection(const char* host, int port) : connection(redisConnect(host, port)) {
        if (connection != nullptr && connection->err) {
            throw OHLCWithRedisException("Failed to connect to Redis: " + std::string(connection->errstr));
        }
    }

    ~RedisConnection() {
        redisFree(connection);
    }

    redisContext* get() const {
        return connection;
    }

private:
    redisContext* connection;
};

class OHLCConsumerServiceImpl final : public ohlc::OHLCConsumerService::Service {
public:
    grpc::Status SendOHLC(grpc::ServerContext* context, const ohlc::OHLC* request, ohlc::SendOHLCResponse* response) override {
        ExceptionHandler<OHLCWithRedisException>::Handle([&]() {
            saveOHLCDataToRedis(request);
            response->set_message("OHLC data received successfully");
        }, "Error saving OHLC data to Redis.");

        return grpc::Status::OK;
    }

    grpc::Status GetOHLC(grpc::ServerContext* context, const ohlc::StockRequest* request, ohlc::OHLC* response) override {
        ExceptionHandler<OHLCWithRedisException>::Handle([&]() {
            retrieveOHLCDataFromRedis(request->stock_code(), response);
        }, "Error retrieving OHLC data from Redis.");

        return grpc::Status::OK;
    }

private:
    RedisConnection redisConnection{"localhost", 6379};

    void saveOHLCDataToRedis(const ohlc::OHLC* ohlcData) {
        std::string formattedData = serializeOHLCData(ohlcData);

        // Save OHLC data to Redis using SET
        redisReply* reply = static_cast<redisReply*>(redisCommand(redisConnection.get(), "SET %s %s", ohlcData->stock_code().c_str(), formattedData.c_str()));

        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            throw OHLCWithRedisException("Failed to save OHLC data to Redis: " + (reply ? std::string(reply->str) : "NULL"));
        } else {
            std::cout << "Saved OHLC data for stock: " << ohlcData->stock_code() << std::endl;
        }

        freeReplyObject(reply);
    }

    void retrieveOHLCDataFromRedis(const std::string& stockCode, ohlc::OHLC* response) {
        // Retrieve OHLC data from Redis using GET
        redisReply* reply = static_cast<redisReply*>(redisCommand(redisConnection.get(), "GET %s", stockCode.c_str()));

        if (reply != nullptr && reply->type == REDIS_REPLY_STRING) {
            deserializeOHLCData(reply->str, response);
            std::cout << "Retrieved OHLC data for stock: " << stockCode << std::endl;
        } else {
            std::cerr << "OHLC data not found for stock: " << stockCode << std::endl;
        }

        freeReplyObject(reply);
    }

    std::string serializeOHLCData(const ohlc::OHLC* ohlcData) {
        std::ostringstream oss;
        oss << ohlcData->stock_code() << "," << ohlcData->open() << "," << ohlcData->high() << ","
            << ohlcData->low() << "," << ohlcData->close() << "," << ohlcData->volume() << ","
            << ohlcData->value();
        return oss.str();
    }

    void deserializeOHLCData(const std::string& serializedData, ohlc::OHLC* response) {
        std::istringstream iss(serializedData);
        std::string token;

        std::getline(iss, token, ',');
        response->set_stock_code(token);

        std::getline(iss, token, ',');
        response->set_open(std::stod(token));

        std::getline(iss, token, ',');
        response->set_high(std::stod(token));

        std::getline(iss, token, ',');
        response->set_low(std::stod(token));

        std::getline(iss, token, ',');
        response->set_close(std::stod(token));

        std::getline(iss, token, ',');
        response->set_volume(std::stod(token));

        std::getline(iss, token, ',');
        response->set_value(std::stod(token));
    }
};

void runServer() {
    std::string server_address("0.0.0.0:50051");
    OHLCConsumerServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

int main() {
    ExceptionHandler<OHLCWithRedisException>::Handle([&]() {
        runServer();
    }, "Error in the main application.");

    return 0;
}
