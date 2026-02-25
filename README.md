# CASPIAN: Configurable Array of Spiking Programmable Independent Adaptive Neurons

[![DOI](https://zenodo.org/badge/1077862723.svg)](https://doi.org/10.5281/zenodo.18750582)

CASPIAN is a neuromorphic hardware model created by Parker Mitchell at Oak Ridge National Laboratory in 2019. Development is currently ongoing, and this repository is not yet a stable codebase. Use at your own risk.

## Model Overview

 - The default model consists of **neurons** as nodes and **synapses** as edges in a directed graph.
 - All computation occurs at integer time granularity.
 - Computation is designed to allow for activity-dependent evaluation. This is to say updates should only occur when there is useful work to do. This principle holds true for both software and hardware implementations of Caspian.
 - The order of operations for neurons:
    1. Apply leak to the neuron charge
    2. Add all new charge to the neuron
    3. Check if charge exceeds a set threshold. If so, issue a fire and reset neuron charge.
 - Neuron Properties:
    - **Threshold** _(Required)_ - This is the amount of charge which must be exceeded in order for a neuron to fire.
    - **Leak** _(Optional)_ - This corresponds the to tau parameter of Caspian's leak model.
    - **Axonal Delay** _(Optional)_ - This is the amount of delay from when a neuron fires to when synapses receive the fire
 - Synapse Properties:
    - **Weight** _(Required)_ - This is the amount of charge transfered when the synapse fires into a neuron.
    - **Synaptic Delay** _(Optional)_ - This is the amount of delay from when a synapse receives a fire to when the neuron will accumulate the charge.

### Leak Model

In order to allow for efficient hardware implementations, Caspian adopts a modified exponential leak model. Leak follows the form 2^(-t/tau) where t is the time since the last fire and tau is a time constant. The tau parameter may be set on a per-neuron basis. Allowable tau values are 1, 2, 4, 8, or 16. These values are expressed as in 2^x form, so the parameter is 0 for tau=1, 1 for tau=2, 2 for tau=4, 3 for tau=8, and 4 for tau=16. A leak value of -1 in software corresponds to leak being disabled for that neuron. 

## Hardware Platforms

Many different FPGA-based hardware platforms are planned. The initial implementation will be a minimalistic implementation called uCaspian targetting the Lattice ice40 UP5k FPGA. This FPGA is incredibly small, low power, and inexpensive. 

### uCaspian

uCaspian is available in a separate repository at <https://github.com/ORNL/uCaspian>.

## Citation

If you use Caspian, please cite the following paper:

J. Parker Mitchell, Catherine D. Schuman, Robert M. Patton, and Thomas E. Potok. 2020. Caspian: A Neuromorphic Development Platform. In Proceedings of the 2020 Annual Neuro-Inspired Computational Elements Workshop (NICE '20). Association for Computing Machinery, New York, NY, USA, Article 8, 1–6. https://doi.org/10.1145/3381755.3381764

```bib
@inproceedings{10.1145/3381755.3381764,
    author = {Mitchell, J. Parker and Schuman, Catherine D. and Patton, Robert M. and Potok, Thomas E.},
    title = {Caspian: A Neuromorphic Development Platform},
    year = {2020},
    isbn = {9781450377188},
    publisher = {Association for Computing Machinery},
    address = {New York, NY, USA},
    url = {https://doi.org/10.1145/3381755.3381764},
    doi = {10.1145/3381755.3381764},
    abstract = {Current neuromorphic systems often may be difficult to use and costly to deploy. There exists a need for a simple yet flexible neuromorphic development platform which can allow researchers to quickly prototype ideas and applications. Caspian offers a high-level API along with a fast spiking simulator to enable the rapid development of neuromorphic solutions. It further offers an FPGA architecture that allows for simplified deployment -- particularly in SWaP (size, weight, and power) constrained environments. Leveraging both software and hardware, Caspian aims to accelerate development and deployment while enabling new researchers to quickly become productive with a spiking neural network system.},
    booktitle = {Proceedings of the 2020 Annual Neuro-Inspired Computational Elements Workshop},
    articleno = {8},
    numpages = {6},
    keywords = {spiking neural network, reconfigurable computing, neuromorphic},
    location = {Heidelberg, Germany},
    series = {NICE '20}
}
```
