//====================================================================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <json/json.h>
#include <cstdlib>
#include <iomanip> // Add this include for formatting output
#include <grpc++/grpc++.h>
#include "ohlc.pb.h"
#include "ohlc.grpc.pb.h"
#include <filesystem>

struct MyOHLC
{
    double open;
    double high;
    double low;
    double close;
    int volume;
    double value;
    // ... (other members remain the same)
    std::vector<double> historicalHighs{std::numeric_limits<double>::lowest()}; // Initialize with a very low value

     double lowestPrice; // Add a member to store the lowest price
};
class OHLCProducer
{
public:
    void processFilesInFolder(const std::string &folderPath)
    {
        // Iterate through all files in the specified folder
        for (const auto &entry : std::filesystem::directory_iterator(folderPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".ndjson")
            {
                processFile(entry.path().string());
            }
        }
    }

    void processFile(const std::string &filePath)
    {
        std::ifstream file(filePath);
        std::string line;

        while (std::getline(file, line))
        {
            // Same processing logic as before
            // Parse the JSON data
            Json::CharReaderBuilder reader;
            Json::Value jsonData;
            std::istringstream iss(line);

            try
            {
                Json::parseFromStream(reader, iss, &jsonData, nullptr);
                char type = jsonData["type"].asString()[0];
                int quantity = 0;   // Initialize quantity
                double price = 0.0; // Initialize price

                // Extract quantity and price based on the "type" field
                if (type == 'A') // Order type
                {
                    quantity = std::stoi(jsonData["quantity"].asString());
                    price = std::stod(jsonData["price"].asString());
                }
                else if (type == 'E') // Executed order type
                {
                    quantity = std::stoi(jsonData["executed_quantity"].asString());
                    price = std::stod(jsonData["execution_price"].asString());
                }

                std::string stockCode = jsonData["stock_code"].asString();

                // Check if the stock code exists in the map
                auto it = ohlcMap.find(stockCode);

                if (it == ohlcMap.end())
                {
                    // If the stock code does not exist, create a new OHLC instance
                    MyOHLC ohlc;
                    ohlc.open = price; // Initial open value is the current execution price
                    ohlc.high = price;
                    ohlc.low = price;
                    ohlc.close = price;
                    ohlc.volume = quantity;
                    ohlc.value = quantity * price;
                    ohlc.historicalHighs.push_back(price);
                    ohlc.lowestPrice = price; // Initialize lowestPrice with the first observed price

                    ohlcMap[stockCode] = ohlc;
                }
                else
                {
                    // If the stock code exists, update the existing entry
                    MyOHLC &ohlc = ohlcMap[stockCode];

                    // Set the open value to the previous execution price for the next data point
                    ohlc.open = ohlc.close;

                    // Update historical highs and lows
                    ohlc.historicalHighs.push_back(price);
              
                    ohlc.high = *std::max_element(ohlc.historicalHighs.begin(), ohlc.historicalHighs.end());
                   
                     // Update lowestPrice if a new lowest price is encountered
                    if (price < ohlc.lowestPrice)
                    {
                        ohlc.low = price;
                    }
                    else
                    {
                         ohlc.low = ohlc.lowestPrice;
                    }


                    // Update close to the current execution price
                    ohlc.close = price;

                    // Update volume and value
                    ohlc.volume += quantity;
                    ohlc.value += quantity * price;
                }
            }
            catch (const Json::Exception &e)
            {
                std::cerr << "Error parsing JSON: " << e.what() << std::endl;
                continue; // Skip to the next iteration
            }
        }
    }

    void sendOHLCDataToConsumer()
    {
        // Create a gRPC channel to communicate with OHLCConsumerService
        std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
        std::unique_ptr<ohlc::OHLCConsumerService::Stub> stub = ohlc::OHLCConsumerService::NewStub(channel);

        // Iterate through OHLC data and send Protobuf messages
        for (const auto &entry : ohlcMap)
        {
            ohlc::OHLC request;
            request.set_stock_code(entry.first);
            request.set_open(entry.second.open);
            request.set_high(entry.second.high);
            request.set_low(entry.second.low);
            request.set_close(entry.second.close);
            request.set_volume(entry.second.volume);
            request.set_value(entry.second.value);

            grpc::ClientContext context;
            ohlc::SendOHLCResponse response;
            grpc::Status status = stub->SendOHLC(&context, request, &response);

            if (status.ok())
            {
                std::cout << "OHLC data sent successfully for stock: " << entry.first << std::endl;
            }
            else
            {
                std::cerr << "Failed to send OHLC data for stock: " << entry.first << ". Error: " << status.error_message() << std::endl;
            }
        }
    }

private:
    std::map<std::string, MyOHLC> ohlcMap;
};

int main()
{
    OHLCProducer producer;
    producer.processFilesInFolder("/BityreTest2/data");
    producer.sendOHLCDataToConsumer();

    return 0;
}
