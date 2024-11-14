#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <vector>

using namespace cv;
using namespace std;
using namespace cv::dnn;

float confThreshold = 0.5; // Confidence threshold
float nmsThreshold = 0.4;  // Non-maximum suppression threshold

// Load YOLOv4 Tiny model and perform object detection
void detectObjects(Mat& frame, Net& net) {
    Mat blob;
    blobFromImage(frame, blob, 1/255.0, Size(416, 416), Scalar(0, 0, 0), true, false);
    net.setInput(blob);

    // Get output layer names
    vector<String> outNames = net.getUnconnectedOutLayersNames();
    vector<Mat> outs;
    net.forward(outs, outNames);

    vector<int> classIds;
    vector<float> confidences;
    vector<Rect> boxes;

    // Process the network output
    for (size_t i = 0; i < outs.size(); i++) {
        float* data = (float*)outs[i].data;
        for (int j = 0; j < outs[i].rows; j++, data += outs[i].cols) {
            Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
            Point classIdPoint;
            double confidence;
            minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
            if (confidence > confThreshold) {
                int centerX = (int)(data[0] * frame.cols);
                int centerY = (int)(data[1] * frame.rows);
                int width = (int)(data[2] * frame.cols);
                int height = (int)(data[3] * frame.rows);
                int left = centerX - width / 2;
                int top = centerY - height / 2;

                classIds.push_back(classIdPoint.x);
                confidences.push_back((float)confidence);
                boxes.push_back(Rect(left, top, width, height));
            }
        }
    }

    // Apply non-maximum suppression to eliminate redundant overlapping boxes with lower confidences
    vector<int> indices;
    dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);
    for (int idx : indices) {
        Rect box = boxes[idx];
        rectangle(frame, box, Scalar(0, 255, 0), 2);
        String label = format("Class %d: %.2f", classIds[idx], confidences[idx]);
        putText(frame, label, Point(box.x, box.y - 10), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 2);
    }
}

int main() 
{
    // Load the YOLOv4 Tiny model
    String modelConfiguration = "yolov4-tiny.cfg";  // 配置文件
    String modelWeights = "yolov4-tiny.weights";    // 權重文件
    Net net = readNetFromDarknet(modelConfiguration, modelWeights);

    // Optional: Specify backend and target (e.g., DNN_BACKEND_OPENCV, DNN_TARGET_CPU)
    net.setPreferableBackend(DNN_BACKEND_OPENCV);
    net.setPreferableTarget(DNN_TARGET_CPU);

    // Load image and perform detection
    Mat frame = imread("example_image.jpg");
    if (frame.empty()) {
        cerr << "Could not read the image." << endl;
        return -1;
    }

    detectObjects(frame, net);

    // Save the result to a file instead of showing it
    String outputFilename = "output_image.jpg";
    imwrite(outputFilename, frame);
    cout << "Output saved as " << outputFilename << endl;

    return 0;
}
