# Guide d'Installation : ROS2 Jazzy & Caméra CSI (IMX219) sur Raspberry Pi 5

**Matériel cible :** Raspberry Pi 5 + Raspberry Pi Camera V2.1 (IMX219) + Nappe Arducam 22-pin vers 15-pin  
**Système d'exploitation :** Ubuntu 24.04 LTS (Noble Numbat) - 64-bit  
**Middleware :** ROS2 Jazzy Jalisco  

---

## 1. Branchement physique et Configuration matérielle (Boot)

### Sens de branchement de la nappe CSI
1. **Côté Raspberry Pi 5 (Port `J3` - `CAM/DISP 0` proche du port Ethernet) :** Insérer l'extrémité fine (22 broches) avec les **contacts métalliques dorés tournés vers le port Ethernet** et le renfort noir isolant (côté texte `Arducam`) contre le loquet mobile noir (côté processeur).
2. **Côté Caméra V2.1 :** Insérer l'extrémité large (15 broches) avec les **contacts métalliques argentés plaqués contre le circuit imprimé (PCB)** de la caméra, et le renfort noir contre le loquet mobile du connecteur blanc.

### Activation de l'overlay matériel
Éditer le fichier de configuration du firmware :

```bash
sudo nano /boot/firmware/config.txt
```

Ajouter ces lignes tout en bas du fichier :

```ini
camera_auto_detect=1
dtoverlay=imx219,cam0
```

Redémarrer la Raspberry Pi 5 pour appliquer la modification :

```bash
sudo reboot
```

Après redémarrage, vérifier que le noyau détecte bien le capteur sur le bus I2C :

```bash
sudo dmesg | grep -i imx219
# Résultat attendu : "Using sensor imx219 10-0010 for capture"
```

---

## 2. Installation de ROS2 Jazzy Jalisco

### Configuration des locales et des dépôts officiels
```bash
sudo apt update && sudo apt install -y locales curl software-properties-common
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

sudo add-apt-repository universe -y
sudo curl -sSL [https://raw.githubusercontent.com/ros/rosdistro/master/ros.key](https://raw.githubusercontent.com/ros/rosdistro/master/ros.key) -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] [http://packages.ros.org/ros2/ubuntu](http://packages.ros.org/ros2/ubuntu) $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
```

### Installation des paquets ROS2, OpenCV et Foxglove
```bash
sudo apt update
sudo apt install -y ros-jazzy-ros-base python3-colcon-common-extensions \
  libopencv-dev ros-jazzy-cv-bridge ros-jazzy-sensor-msgs \
  ros-jazzy-foxglove-bridge
```

Ajouter le chargement automatique de ROS2 dans le `.bashrc` :
```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

---

## 3. Compilation de `libpisp` et `libcamera` (Spécifique Pi 5)

*Note : La version par défaut de `libcamera` sous Ubuntu 24.04 (`v0.2.0`) ne supporte pas le processeur d'image matériel (PiSP) de la Raspberry Pi 5. Il faut compiler la branche officielle Raspberry Pi.*

### Préparation et dépendances de compilation
```bash
sudo apt remove -y libcamera-v4l2 libcamera-tools libcamera-ipa
sudo apt update
sudo apt install -y git python3-pip python3-jinja2 python3-yaml python3-ply \
  libboost-dev libgnutls28-dev openssl libtiff-dev pybind11-dev \
  meson ninja-build pkg-config libglib2.0-dev libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev libudev-dev libyaml-dev nlohmann-json3-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good
```

### Compilation de `libpisp`
```bash
cd ~
git clone [https://github.com/raspberrypi/libpisp.git](https://github.com/raspberrypi/libpisp.git)
cd libpisp
meson setup build --prefix=/usr/local
ninja -C build
sudo ninja -C build install
sudo ldconfig
```

### Compilation de `libcamera` (avec support PiSP et GStreamer)
```bash
cd ~
git clone [https://github.com/raspberrypi/libcamera.git](https://github.com/raspberrypi/libcamera.git)
cd libcamera
meson setup build --buildtype=release --prefix=/usr/local \
  -Dpipelines=rpi/pisp \
  -Dipas=rpi/pisp \
  -Dv4l2=true \
  -Dgstreamer=enabled \
  -Dlc-compliance=disabled \
  -Dcam=enabled \
  -Dqcam=disabled \
  -Ddocumentation=disabled \
  -Dpycamera=disabled

ninja -C build
sudo ninja -C build install
sudo ldconfig
```

---

## 4. Permissions Matérielles (Accès sans `sudo`)

Débloquer l'accès au DMA (`dma_heap`) et au GPU pour l'utilisateur courant, et déclarer le plugin GStreamer :

```bash
# Ajout aux groupes matériels
sudo usermod -aG video,render $USER

# Création de la règle udev pour le PiSP
echo 'SUBSYSTEM=="dma_heap", GROUP="video", MODE="0660"' | sudo tee /etc/udev/rules.d/99-dma-heap.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo chmod 660 /dev/dma_heap/*
sudo chgrp video /dev/dma_heap/*

# Déclaration du chemin GStreamer dans l'environnement
echo 'export GST_PLUGIN_PATH=/usr/local/lib/aarch64-linux-gnu/gstreamer-1.0' >> ~/.bashrc
source ~/.bashrc
```

### Test de validation matérielle
```bash
/usr/local/bin/cam -l
/usr/local/bin/cam -c 1 --capture=10
```

---

## 5. Création et Compilation du Package ROS2 `camera_vision`

### Création de l'espace de travail et du package
```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
ros2 pkg create --build-type ament_cmake camera_vision
```

### Écriture du nœud C++ (`src/camera_publisher.cpp`)
```bash
cat << 'EOF' > ~/ros2_ws/src/camera_vision/src/camera_publisher.cpp
#include <chrono>
#include <memory>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/header.hpp"
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>

using namespace std::chrono_literals;

class CameraPublisher : public rclcpp::Node
{
public:
    CameraPublisher() : Node("camera_publisher")
    {
        publisher_ = create_publisher<sensor_msgs::msg::Image>("/camera/image_raw", 10);

        std::string pipeline =
            "libcamerasrc ! "
            "video/x-raw, width=640, height=480, framerate=30/1, format=BGR ! "
            "videoconvert ! "
            "video/x-raw, format=BGR ! "
            "appsink drop=true max-buffers=1";

        cap_.open(pipeline, cv::CAP_GSTREAMER);

        if (!cap_.isOpened()) {
            RCLCPP_ERROR(get_logger(), "Echec de l'ouverture du pipeline GStreamer libcamerasrc !");
            return;
        }

        RCLCPP_INFO(get_logger(), "Camera CSI IMX219 demarree (640x480 @ 30Hz via PiSP).");

        timer_ = create_wall_timer(33ms, [this]() { timer_callback(); });
    }

private:
    void timer_callback()
    {
        cv::Mat frame;
        if (!cap_.read(frame) || frame.empty()) {
            RCLCPP_WARN(get_logger(), "Image vide ou perdue !");
            return;
        }

        std_msgs::msg::Header header;
        header.stamp = get_clock()->now();
        header.frame_id = "camera_frame";

        auto msg = cv_bridge::CvImage(header, "bgr8", frame).toImageMsg();
        publisher_->publish(*msg);
    }

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr publisher_;
    cv::VideoCapture cap_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CameraPublisher>());
    rclcpp::shutdown();
    return 0;
}
EOF
```

### Configuration de `package.xml`
```bash
cat << 'EOF' > ~/ros2_ws/src/camera_vision/package.xml
<?xml version="1.0"?>
<?xml-model href="[http://download.ros.org/schema/package_format3.xsd](http://download.ros.org/schema/package_format3.xsd)" schematypens="[http://www.w3.org/2001/XMLSchema](http://www.w3.org/2001/XMLSchema)"?>
<package format="3">
  <name>camera_vision</name>
  <version>0.0.0</version>
  <description>Noeud de publication camera CSI IMX219 pour Pi 5</description>
  <maintainer email="sae@todo.todo">sae</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>

  <depend>rclcpp</depend>
  <depend>sensor_msgs</depend>
  <depend>std_msgs</depend>
  <depend>cv_bridge</depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
EOF
```

### Configuration de `CMakeLists.txt`
```bash
cat << 'EOF' > ~/ros2_ws/src/camera_vision/CMakeLists.txt
cmake_minimum_required(VERSION 3.8)
project(camera_vision)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(camera_publisher src/camera_publisher.cpp)

ament_target_dependencies(camera_publisher
  rclcpp
  sensor_msgs
  std_msgs
  cv_bridge
  OpenCV
)

install(TARGETS
  camera_publisher
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
EOF
```

### Compilation du workspace
```bash
cd ~/ros2_ws
colcon build --packages-select camera_vision
echo "source ~/ros2_ws/install/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

---

## 6. Exécution et Visualisation à distance (Foxglove)

* **Terminal 1 (Publication du flux caméra) :**
  ```bash
  ros2 run camera_vision camera_publisher
  ```

* **Terminal 2 (Pont WebSocket pour Foxglove Studio) :**
  ```bash
  ros2 launch foxglove_bridge foxglove_bridge_launch.xml
  ```

* **Terminal 3 (Optionnel - Vérification de la fréquence à 30 Hz) :**
  ```bash
  ros2 topic hz /camera/image_raw
  ```

* **Sur le PC de développement :** Ouvrir **Foxglove Studio**, se connecter via *Foxglove WebSocket* à l'adresse `ws://<IP_DE_LA_PI5>:8765` et afficher le topic `/camera/image_raw`.
