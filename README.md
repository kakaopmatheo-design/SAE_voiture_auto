# Véhicule Autonome 1/10 (SAE) — Guide d'Installation Raspberry Pi 5

**Matériel :** Raspberry Pi 5 + Raspberry Pi Camera V2.1 (IMX219) + Nappe CSI 22-pin vers 15-pin  
**OS :** Ubuntu 24.04 LTS (64-bit)  
**Middleware :** ROS2 Jazzy Jalisco  

---

## 1. Branchement et Configuration Matérielle (CSI)

### Orientation de la nappe CSI
* **Côté Raspberry Pi 5 (Port `J3` / `CAM/DISP 0` près du port Ethernet) :** Contacts métalliques dorés tournés **vers le port Ethernet**, renfort noir isolant contre le loquet plastique (côté processeur).
* **Côté Caméra V2.1 :** Contacts métalliques argentés plaqués **contre le PCB de la caméra**, renfort noir contre le loquet mobile du connecteur blanc.

### Activation du capteur dans le firmware
Éditer `/boot/firmware/config.txt` :
```bash
sudo nano /boot/firmware/config.txt
```
Ajouter en fin de fichier :
```ini
camera_auto_detect=1
dtoverlay=imx219,cam0
```
Redémarrer puis vérifier la détection matérielle sur le bus I2C :
```bash
sudo reboot
sudo dmesg | grep -i imx219
# Doit afficher : "Using sensor imx219 10-0010 for capture"
```

---

## 2. Installation de ROS2 Jazzy

### Configuration des dépôts
```bash
sudo apt update && sudo apt install -y locales curl software-properties-common
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

sudo add-apt-repository universe -y
sudo curl -sSL [https://raw.githubusercontent.com/ros/rosdistro/master/ros.key](https://raw.githubusercontent.com/ros/rosdistro/master/ros.key) -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] [http://packages.ros.org/ros2/ubuntu](http://packages.ros.org/ros2/ubuntu) $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
```

### Installation des paquets ROS2 et OpenCV
```bash
sudo apt update
sudo apt install -y ros-jazzy-ros-base python3-colcon-common-extensions \
  libopencv-dev ros-jazzy-cv-bridge ros-jazzy-sensor-msgs \
  ros-jazzy-foxglove-bridge

echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

---

## 3. Compilation des Pilotes Caméra Pi 5 (`libpisp` & `libcamera`)

Ubuntu 24.04 fournit `libcamera v0.2.0` par défaut, qui ne supporte pas le processeur d'image matériel (PiSP) de la Pi 5. Il faut compiler la version officielle Raspberry Pi.

### Suppression des anciens paquets et installation des dépendances
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

### Compilation de `libcamera`
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

## 4. Permissions Utilisateur et Environnement GStreamer

Autoriser l'accès au DMA (`dma_heap`) sans `sudo` et déclarer le plugin GStreamer compilé :

```bash
sudo usermod -aG video,render $USER

echo 'SUBSYSTEM=="dma_heap", GROUP="video", MODE="0660"' | sudo tee /etc/udev/rules.d/99-dma-heap.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo chmod 660 /dev/dma_heap/*
sudo chgrp video /dev/dma_heap/*

echo 'export GST_PLUGIN_PATH=/usr/local/lib/aarch64-linux-gnu/gstreamer-1.0' >> ~/.bashrc
source ~/.bashrc
```

Vérifier la détection et l'acquisition :
```bash
/usr/local/bin/cam -l
/usr/local/bin/cam -c 1 --capture=10
```

---

## 5. Récupération et Compilation du Projet ROS2

Cloner ce dépôt sur une nouvelle Raspberry Pi 5 et compiler l'espace de travail :

```bash
git clone git@github.com:kakaopmatheo-design/SAE_voiture_auto.git ~/ros2_ws
cd ~/ros2_ws
colcon build
echo "source ~/ros2_ws/install/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

---

## 6. Lancement et Visualisation

* **Lancer le nœud caméra CSI (30 Hz) :**
  ```bash
  ros2 run camera_vision camera_publisher
  ```

* **Lancer le serveur Foxglove (dans un 2e terminal) :**
  ```bash
  ros2 launch foxglove_bridge foxglove_bridge_launch.xml
  ```

* **Vérifier la fréquence de publication :**
  ```bash
  ros2 topic hz /camera/image_raw
  ```

* **Visualisation PC :** Ouvrir **Foxglove Studio** et se connecter en WebSocket sur `ws://<IP_PI5>:8765`.
