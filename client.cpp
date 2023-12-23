#include <grpc++/grpc++.h>
#include "ohlc.grpc.pb.h"
#include <iostream>
#include <stdexcept>

template <typename ExceptionType>
class ExceptionHandler {
public:
    template <typename Func>
    static void Handle(Func func, const std::string& errorMessage = "An error occurred.") {
        try {
            func();
        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            throw ExceptionType(errorMessage + " " + e.what());
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

using OHLCWithGrpcException = std::runtime_error;

class OHLCClient {
public:
    OHLCClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(ohlc::OHLCConsumerService::NewStub(channel)) {}

    ohlc::OHLC getOHLCData(const std::string& stockCode) {
        ohlc::StockRequest request;
        request.set_stock_code(stockCode);

        ohlc::OHLC response;
        grpc::ClientContext context;

        ExceptionHandler<OHLCWithGrpcException>::Handle([&]() {
            grpc::Status status = stub_->GetOHLC(&context, request, &response);

            if (!status.ok()) {
                throw OHLCWithGrpcException("Error getting OHLC data for stock: " + stockCode + ". Error: " + status.error_message());
            }

            std::cout << "Received OHLC data for stock: " << stockCode << std::endl;
        }, "Error communicating with gRPC server.");

        return response;
    }

    void displayOHLCData(const ohlc::OHLC& ohlcData) {
        // Add your code to display the OHLC data as needed
        std::cout << "OHLC Data:\n"
                  << "  Stock Code: " << ohlcData.stock_code() << "\n"
                  << "  Open: " << ohlcData.open() << "\n"
                  << "  High: " << ohlcData.high() << "\n"
                  << "  Low: " << ohlcData.low() << "\n"
                  << "  Close: " << ohlcData.close() << "\n"
                  << "  Volume: " << ohlcData.volume() << "\n"
                  << "  Value: " << ohlcData.value() << std::endl;
    }

private:
    std::unique_ptr<ohlc::OHLCConsumerService::Stub> stub_;
};

int main(int argc, char** argv) {
    ExceptionHandler<OHLCWithGrpcException>::Handle([&]() {
        if (argc != 2) {
            throw std::invalid_argument("Usage: " + std::string(argv[0]) + " <stock_code>");
        }

        // Extract stock code from command-line arguments
        std::string stockCode = argv[1];

        // Create a gRPC channel to communicate with the server
        std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());

        // Check if the channel is connected
        if (!channel) {
            throw OHLCWithGrpcException("Error connecting to the server.");
        }

        OHLCClient client(channel);

        // Get OHLC data for the provided stock code
        ohlc::OHLC ohlcData = client.getOHLCData(stockCode);

        // Display the received OHLC data
        client.displayOHLCData(ohlcData);
    }, "Error in the main application.");

    return 0;
}
