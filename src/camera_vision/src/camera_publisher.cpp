#include <chrono>
#include <memory>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/header.hpp"
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>

using namespace std::chrono_literals;

class CameraPublisher : public rclcpp::Node //Creation de la class camerapublisher qui herite de rclcpp::Node /obligatoire
{
public:
    CameraPublisher() : Node("camera_publisher")
    {
        publisher_ = this->create_publisher<sensor_msgs::msg::Image>("/camera/image_raw", 10);

        // Pipeline GStreamer utilisant le processeur d'image PiSP de la Pi 5
        std::string pipeline =
            "libcamerasrc ! "
            "video/x-raw, width=640, height=480, framerate=30/1, format=BGR ! "
            "videoconvert ! "
            "video/x-raw, format=BGR ! "
            "appsink drop=true max-buffers=1";

        cap_.open(pipeline, cv::CAP_GSTREAMER);

        if (!cap_.isOpened()) {
            RCLCPP_ERROR(this->get_logger(), "Échec de l'ouverture du pipeline GStreamer libcamerasrc !");
            return;
        }

        RCLCPP_INFO(this->get_logger(), "Caméra CSI IMX219 démarrée (640x480 @ 30Hz via PiSP).");

        timer_ = this->create_wall_timer(
            33ms, [this]() { this->timer_callback(); });
    }

private:
    void timer_callback()
    {
        cv::Mat frame;
        if (!cap_.read(frame) || frame.empty()) {
            RCLCPP_WARN(this->get_logger(), "Image vide ou perdue !");
            return;
        }

        std_msgs::msg::Header header;
        header.stamp = this->get_clock()->now();
        header.frame_id = "camera_frame";

        auto msg = cv_bridge::CvImage(header, "bgr8", frame).toImageMsg();
        publisher_->publish(*msg);
    }

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr publisher_; //deeclaration des variables private en pointeur smart
    cv::VideoCapture cap_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);                         // Démarre ROS2
    rclcpp::spin(std::make_shared<CameraPublisher>()); // Fait tourner le nœud en boucle
    rclcpp::shutdown();                               // Ferme proprement au Ctrl+C
    return 0;
}