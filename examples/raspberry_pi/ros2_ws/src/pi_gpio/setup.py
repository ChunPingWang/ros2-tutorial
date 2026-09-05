from setuptools import find_packages, setup

package_name = 'pi_gpio'

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
    description='Raspberry Pi GPIO example node for ROS 2 (LED subscriber, button publisher).',
    license='MIT',
    entry_points={
        'console_scripts': [
            'gpio_node = pi_gpio.gpio_node:main',
        ],
    },
)
