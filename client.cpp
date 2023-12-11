#include <grpc++/grpc++.h>
#include "ohlc.grpc.pb.h"

class OHLCClient {
public:
    OHLCClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(ohlc::OHLCConsumerService::NewStub(channel)) {}

    ohlc::OHLC getOHLCData(const std::string& stockCode) {
        ohlc::StockRequest request;
        request.set_stock_code(stockCode);

        ohlc::OHLC response;
        grpc::ClientContext context;

        grpc::Status status = stub_->GetOHLC(&context, request, &response);

        if (status.ok()) {
            std::cout << "Received OHLC data for stock: " << stockCode << std::endl;
        } else {
            std::cerr << "Error getting OHLC data for stock: " << stockCode << ". Error: " << status.error_message() << std::endl;
            exit(1);  // Exit with an error code
        }

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
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <stock_code>" << std::endl;
        return 1;
    }

    // Extract stock code from command-line arguments
    std::string stockCode = argv[1];

    // Create a gRPC channel to communicate with the server
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());

    // Check if the channel is connected
    if (!channel) {
        std::cerr << "Error connecting to the server." << std::endl;
        return 1;
    }

    OHLCClient client(channel);

    // Get OHLC data for the provided stock code
    ohlc::OHLC ohlcData = client.getOHLCData(stockCode);

    // Display the received OHLC data
    client.displayOHLCData(ohlcData);

    return 0;
}
