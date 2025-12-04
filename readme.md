# 20-20-20 提醒

每个20分钟全屏显示一个始终20s，提醒你该休息了，默认配置文件路径

～/.config/tttreminder.ini


# 编译

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev

git clone https://github.com/diandianti/twentytwentytwentyreminder.git
cd twentytwentytwentyreminder
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./twentytwentytwentyreminder -c ../tttreminder.ini
```

# 效果

[效果图](demo.png)