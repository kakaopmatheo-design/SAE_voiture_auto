// --- 1. C++ Standard ---
#include <memory>   // Pointeurs intelligents (SharedPtr, make_shared)
#include <vector>   // Tableaux dynamiques (indispensable pour stocker les contours OpenCV)
#include <cmath>    // Calculs mathématiques ( distances, angles : sqrt, atan2, abs)

// --- 2. ROS2 et Messages ---
#include "rclcpp/rclcpp.hpp"               // Cœur de ROS2 (Node, Subscriber, Publisher, Paramètres)
#include "sensor_msgs/msg/image.hpp"       // Format d'image ROS2 (entrée et image de debug)
#include "geometry_msgs/msg/point.hpp"     // Pour publier la position (x, y, distance) d'un obstacle
#include <cv_bridge/cv_bridge.hpp>         // Traducteur Image ROS2 <-> Matrice OpenCV

// --- 3. OpenCV (Vision Classique uniquement) ---
#include <opencv2/core.hpp>                // Matrices (cv::Mat), Points (cv::Point), Rectangles (cv::Rect)
#include <opencv2/imgproc.hpp>             // Traitement : HSV (cvtColor), seuillage (inRange), contours (findContours)


#include "sensor_msgs/msg/compressed_image.hpp" // Pour envoyer en JPEG sans lag Wi-Fi
#include <opencv2/imgcodecs.hpp>                // Pour la fonction cv::imencode(".jpg")

class ObstacleDetector : public rclcpp::Node
{
public:
    ObstacleDetector() : Node("obstacle_detector")
    {
        subscription_ = create_subscription<sensor_msgs::msg::Image>("/camera/image_raw", 1,[this](const sensor_msgs::msg::Image::SharedPtr msg)
        {
            image_treatement(msg);
        });
        // Dans le constructeur (remplace ton publisher_ actuel) :
        publisher_ = create_publisher<sensor_msgs::msg::CompressedImage>("/camera/debug_image/compressed", 1);
    }
private:
    void image_treatement(const sensor_msgs::msg::Image::SharedPtr msg)
    {
        cv::Mat no_treated_image = cv_bridge::toCvCopy(msg, "bgr8")->image;
        cv::Mat treated_image;
        cv::Mat mask;

        // 1. Conversion BGR -> HSV
        cv::cvtColor(no_treated_image, treated_image, cv::COLOR_BGR2HSV);

        // --- VISEUR DE CALIBRATION AU CENTRE (x=320, y=240) ---
        cv::Vec3b pixel_centre = treated_image.at<cv::Vec3b>(240, 320);
        int h = pixel_centre[0]; // Teinte (0 à 179)
        int s = pixel_centre[1]; // Saturation (0 à 255)
        int v = pixel_centre[2]; // Luminosité (0 à 255)

        // Dessine un petit cercle bleu au centre de l'écran pour viser ton obstacle
        cv::circle(no_treated_image, cv::Point(320, 240), 5, cv::Scalar(255, 0, 0), 2);
        
        // Affiche les valeurs HSV de l'objet visé toutes les 1 seconde dans le terminal
        RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 1000, 
            "Couleur au centre -> H: %d | S: %d | V: %d", h, s, v);

        // 2. Seuillage et Contours (Mets ici les valeurs H, S, V lues dans le terminal +/- 15)
        cv::Scalar couleur_min(120, 210, 45);   // H_min, S_min, V_min
        cv::Scalar couleur_max(180, 240, 70); // H_max, S_max, V_max
        cv::inRange(treated_image, couleur_min, couleur_max, mask);
        cv::erode(mask, mask, cv::Mat());

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        for (const auto& contour : contours) {
            if (cv::contourArea(contour) > 300) {
                cv::Rect boite = cv::boundingRect(contour);
                cv::rectangle(no_treated_image, boite, cv::Scalar(0, 255, 0), 2);
            }
        }

        // 3. Compression JPEG ultra-rapide pour Foxglove (divise la taille par 30)
        sensor_msgs::msg::CompressedImage debug_msg;
        debug_msg.header = msg->header;
        debug_msg.format = "jpeg";
        cv::imencode(".jpg", no_treated_image, debug_msg.data, {cv::IMWRITE_JPEG_QUALITY, 70});
        publisher_->publish(debug_msg);
    }

    rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr subscription_;

};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);                         // Démarre ROS2
    rclcpp::spin(std::make_shared<ObstacleDetector>()); // Fait tourner le nœud en boucle
    rclcpp::shutdown();                               // Ferme proprement au Ctrl+C
    return 0;
}