# UU-SnowSensing

This repository contains hardware and software designs for low(er) cost snow sensing stations. These next-generation snow monitoring stations enable collection of near real-time, physiographically representative water and energy balance observations in montane catchments. The hardware is designed for implementing robust, low-cost, low-power snow monitoring networks to enhance the spatial representation of data required for modeling snow accumulation, distribution, and melt. Stations are meant to complement existing snow monitoring infrastructure (e.g., NRCS SNOTEL stations) and enable establishment of sensing locations that represent different combinations of aspect, slope, vegetation, and elevation. Sensors are intended to capture key snow-energy-balance processes, including: snow depth, incoming and outgoing short- and long-wave radiation, soil moisture at multiple depths, and air temperature. 

## Getting Started

Follow our [guide for getting started](docs/getting_started.md) to construct and program a snow sensing station. This guide will lead you through:
* Purchasing components necessary for constructing a station
* Setting up and wiring the hardware for a station
* Programming your station for data collection and communication

## Repository Structure

### hardware

This folder contains designs and specifications for sensors, dataloggers, communication peripherals, and components required for building a snow sensing station.

### code

This folder contains the code used to operate the snow sensing stations, including data collection, data storage, and data communication. It also includes necessary libraries, guidance on programming XBee modules, helpful sketches, and more. 
