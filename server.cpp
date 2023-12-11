//========================================================================================================
#include <grpc++/grpc++.h>
#include "ohlc.grpc.pb.h"
#include <hiredis/hiredis.h>

class OHLCConsumerServiceImpl final : public ohlc::OHLCConsumerService::Service
{
public:
    grpc::Status SendOHLC(grpc::ServerContext *context, const ohlc::OHLC *request, ohlc::SendOHLCResponse *response) override
    {
        // Save OHLC data to Redis
        saveOHLCDataToRedis(request);

        // You might want to include an acknowledgment or status message in the response
        response->set_message("OHLC data received successfully");

        return grpc::Status::OK;
    }

    grpc::Status GetOHLC(grpc::ServerContext *context, const ohlc::StockRequest *request, ohlc::OHLC *response) override
    {
        // Retrieve OHLC data from Redis based on stock code
        retrieveOHLCDataFromRedis(request->stock_code(), response);

        return grpc::Status::OK;
    }

private:
    // Redis connection

    redisContext *redisConnection;
    void saveOHLCDataToRedis(const ohlc::OHLC *ohlcData)
    {
        // Establish connection to Redis server
        redisConnection = redisConnect("localhost", 6379);

        if (redisConnection != nullptr && redisConnection->err)
        {
            std::cerr << "Failed to connect to Redis: " << redisConnection->errstr << std::endl;
            return;
        }

        // Convert OHLC data to a formatted string
        std::string formattedData = serializeOHLCData(ohlcData);

        // Save OHLC data to Redis using SET
        redisReply *reply = static_cast<redisReply *>(redisCommand(redisConnection, "SET %s %s", ohlcData->stock_code().c_str(), formattedData.c_str()));

        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
        {
            std::cerr << "Failed to save OHLC data to Redis: " << (reply ? reply->str : "NULL") << std::endl;
        }
        else
        {
            std::cout << "Saved OHLC data for stock: " << ohlcData->stock_code() << std::endl;
        }

        freeReplyObject(reply);
        redisFree(redisConnection);
    }

    void retrieveOHLCDataFromRedis(const std::string &stockCode, ohlc::OHLC *response)
    {
        // Establish connection to Redis server
        redisConnection = redisConnect("localhost", 6379);

        if (redisConnection != nullptr && redisConnection->err)
        {
            std::cerr << "Failed to connect to Redis: " << redisConnection->errstr << std::endl;
            return;
        }

        // Retrieve OHLC data from Redis using GET
        redisReply *reply = static_cast<redisReply *>(redisCommand(redisConnection, "GET %s", stockCode.c_str()));

        if (reply != nullptr && reply->type == REDIS_REPLY_STRING)
        {
            deserializeOHLCData(reply->str, response);
            std::cout << "Retrieved OHLC data for stock: " << stockCode << std::endl;
        }
        else
        {
            std::cerr << "OHLC data not found for stock: " << stockCode << std::endl;
        }

        freeReplyObject(reply);
        redisFree(redisConnection);
    }

    std::string serializeOHLCData(const ohlc::OHLC *ohlcData)
    {
        // Serialize OHLC data to a formatted string
        return ohlcData->stock_code() + "," + std::to_string(ohlcData->open()) + "," +
               std::to_string(ohlcData->high()) + "," + std::to_string(ohlcData->low()) + "," +
               std::to_string(ohlcData->close()) + "," + std::to_string(ohlcData->volume()) + "," + std::to_string(ohlcData->value());
    }

    void deserializeOHLCData(const std::string &serializedData, ohlc::OHLC *response)
    {
        // Deserialize OHLC data from a formatted string
        std::istringstream iss(serializedData);
        std::string token;

        // Split the string by commas and set the values in the response
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

void runServer()
{
    std::string server_address("0.0.0.0:50051");
    OHLCConsumerServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

int main()
{
    runServer();
    return 0;
}
