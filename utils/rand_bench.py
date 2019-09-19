#!/usr/bin/env python3
import time
import argparse
import numpy as np
import caspian

def run_test(inputs, outputs, hidden, runs, seed, runtime):
    n_neurons = inputs + outputs + hidden

    n_input_synapses = min(hidden, 64)
    n_output_synapses = min(hidden, 64)
    n_hidden_synapses = min(hidden, 16)
    n_hidden_synapses_max = n_hidden_synapses * 2

    ####

    trnet_s = time.perf_counter()

    net = caspian.Network(n_neurons)
    net.make_random(n_inputs = inputs, 
                    n_outputs = outputs, 
                    seed = seed,
                    n_input_synapses = n_input_synapses,
                    n_output_synapses = n_output_synapses,
                    n_hidden_synapses = n_hidden_synapses,
                    n_hidden_synapses_max = n_hidden_synapses_max,
                    threshold_range = (0, 255),
                    weight_range = (0, 255),
                    delay_range = (0, 15),
                    leak_range = (0, 3))

    trnet_e = time.perf_counter()

    ####

    tcfg_s = time.perf_counter()
    
    sim = caspian.Simulator()
    sim.configure(net)

    for i in range(outputs):
        sim.monitor_output(i, 0, -1)

    tcfg_e = time.perf_counter()

    print("Seed: {} | Inputs: {} Outputs: {} | Neurons: {} Synapses: {} | Cycles: {}".format(
        seed, inputs, outputs, net.num_neurons(), net.num_synapses(), runtime))

    print("Random Net: {:.1f} us".format((trnet_e - trnet_s) * 10e6))
    print("Configure: {:.1f} us".format((tcfg_e - tcfg_s) * 10e6))

    # track simulation times
    sim_times = list()

    # do specified number of simulation runs
    for run in range(runs):

        # Apply inputs
        for i in range(inputs):
            sim.apply_input(i, 500, i)

        tsim_s = time.perf_counter()
        sim.simulate(runtime)
        accumulations = sim.get_metric("accumulate_count")
        tsim_e = time.perf_counter()

        sim_time = tsim_e - tsim_s

        print("Simulate {:3}: {:.9f} s".format(run, sim_time))

        sim_times.append(sim_time)
        sim.clear_activity()

    sim_times.sort()
    avg = np.sum(sim_times) / runs

    print("Average Simulate (s)     : {}".format(avg))
    print("Median Simulate (s)      : {}".format(np.median(sim_times)))
    print("Accumulations            : {}".format(accumulations))
    print("Accumulations per second : {:.1f}".format(accumulations / avg))
    print("Effective Clock Speed    : {:.4f} KHz".format( (runtime / avg) / 1000 ))

parser = argparse.ArgumentParser(description='Random Network Benchmark')
parser.add_argument('inputs',  metavar='I', type=int, help='number of input neurons')
parser.add_argument('outputs', metavar='O', type=int, help='number of output neurons')
parser.add_argument('hidden',  metavar='H', type=int, help='number of hidden neurons')
parser.add_argument('runs',    metavar='R', type=int, help='number of runs to complete')
parser.add_argument('runtime', metavar='T', type=int, help='number of cycles to simulate per run')
parser.add_argument('seed',    metavar='S', type=int, help='RNG seed value')

args = parser.parse_args()

run_test(inputs = args.inputs,
         outputs = args.outputs,
         hidden = args.hidden,
         runs = args.runs,
         runtime = args.runtime,
         seed = args.seed)
