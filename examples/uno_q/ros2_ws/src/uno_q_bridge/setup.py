from setuptools import find_packages, setup

package_name = 'uno_q_bridge'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='ChunPing Wang',
    maintainer_email='cping.wang.2068@gmail.com',
    description='ROS 2 bridge node for the Arduino UNO Q MCU via the App Lab Bridge RPC.',
    license='MIT',
    entry_points={
        'console_scripts': [
            'bridge_node = uno_q_bridge.bridge_node:main',
        ],
    },
)
