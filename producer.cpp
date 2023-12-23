#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <json/json.h>
#include <cstdlib>
#include <iomanip>
#include <grpc++/grpc++.h>
#include "ohlc.pb.h"
#include "ohlc.grpc.pb.h"
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace fs = std::filesystem;

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

template <typename ExceptionType>
struct MyOHLC {
    double open;
    double high;
    double low;
    double close;
    int volume;
    double value;
    std::vector<double> historicalHighs{std::numeric_limits<double>::lowest()};
    double lowestPrice;
};

class CustomException : public std::exception {
public:
    explicit CustomException(const std::string& message) : message(message) {}

    const char* what() const noexcept override {
        return message.c_str();
    }

private:
    std::string message;
};

using MyOHLCWithException = MyOHLC<CustomException>;

class OHLCProducer {
public:
    void processFilesInFolder(const std::string& folderPath) {
        ExceptionHandler<CustomException>::Handle([&]() {
            for (const auto& entry : fs::directory_iterator(folderPath)) {
                processFile(entry.path().string());
            }
        }, "Error processing folder.");
    }

private:
    void processFile(const std::string& filePath) {
        ExceptionHandler<CustomException>::Handle([&]() {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file: " + filePath);
            }

            std::string line;
            while (std::getline(file, line)) {
                processJSONData(line);
            }
        }, "Error processing file: " + filePath);
    }

    void processJSONData(const std::string& jsonDataStr) {
        ExceptionHandler<CustomException>::Handle([&]() {
            Json::CharReaderBuilder reader;
            Json::Value jsonData;
            std::istringstream iss(jsonDataStr);

            parseJSON(reader, iss, jsonData);

            char type = jsonData["type"].asString()[0];
            int quantity = 0;
            double price = 0.0;

            if (type == 'A') {
                quantity = std::stoi(jsonData["quantity"].asString());
                price = std::stod(jsonData["price"].asString());
            } else if (type == 'E') {
                quantity = std::stoi(jsonData["executed_quantity"].asString());
                price = std::stod(jsonData["execution_price"].asString());
            }

            std::string stockCode = jsonData["stock_code"].asString();

            if (auto it = ohlcMap.find(stockCode); it == ohlcMap.end()) {
                ohlcMap[stockCode] = createOHLC(price, quantity);
            } else {
                updateOHLC(it->second, price, quantity);
            }
        }, "Error processing JSON data.");
    }

    void parseJSON(Json::CharReaderBuilder& reader, std::istringstream& iss, Json::Value& jsonData) {
        Json::parseFromStream(reader, iss, &jsonData, nullptr);
    }

    MyOHLCWithException createOHLC(double price, int quantity) {
        return {
            .open = price,
            .high = price,
            .low = price,
            .close = price,
            .volume = quantity,
            .value = quantity * price,
            .historicalHighs = {price},
            .lowestPrice = price
        };
    }

    void updateOHLC(MyOHLCWithException& ohlc, double price, int quantity) {
        ohlc.open = ohlc.close;
        ohlc.historicalHighs.push_back(price);
        ohlc.high = *std::max_element(ohlc.historicalHighs.begin(), ohlc.historicalHighs.end());

        if (price < ohlc.lowestPrice) {
            ohlc.low = ohlc.lowestPrice = price;
        } else {
            ohlc.low = ohlc.lowestPrice;
        }

        ohlc.close = price;
        ohlc.volume += quantity;
        ohlc.value += quantity * price;
    }

public:
    void sendOHLCDataToConsumer() {
        ExceptionHandler<CustomException>::Handle([&]() {
            std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
            std::unique_ptr<ohlc::OHLCConsumerService::Stub> stub = ohlc::OHLCConsumerService::NewStub(channel);

            for (const auto& [stockCode, ohlc] : ohlcMap) {
                ohlc::OHLC request;
                fillOHLCProtobuf(ohlc, stockCode, request);

                grpc::ClientContext context;
                ohlc::SendOHLCResponse response;
                grpc::Status status = stub->SendOHLC(&context, request, &response);

                handleGRPCStatus(status, stockCode);
            }
        }, "Error sending OHLC data to consumer.");
    }

private:
    void fillOHLCProtobuf(const MyOHLCWithException& ohlc, const std::string& stockCode, ohlc::OHLC& request) {
        request.set_stock_code(stockCode);
        request.set_open(ohlc.open);
        request.set_high(ohlc.high);
        request.set_low(ohlc.low);
        request.set_close(ohlc.close);
        request.set_volume(ohlc.volume);
        request.set_value(ohlc.value);
    }

    void handleGRPCStatus(const grpc::Status& status, const std::string& stockCode) {
        if (status.ok()) {
            std::cout << "OHLC data sent successfully for stock: " << stockCode << std::endl;
        } else {
            std::cerr << "Failed to send OHLC data for stock: " << stockCode << ". Error: " << status.error_message() << std::endl;
        }
    }

private:
    std::map<std::string, MyOHLCWithException> ohlcMap;
};

int main() {
    ExceptionHandler<CustomException>::Handle([&]() {
        OHLCProducer producer;
        producer.processFilesInFolder("./data");
        producer.sendOHLCDataToConsumer();
    }, "An error occurred in the main application.");

    return 0;
}
