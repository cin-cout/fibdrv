import subprocess
import numpy as np
import matplotlib.pyplot as plt
runs = 100

def outlier_filter(data, threshold = 2):
    z = np.abs((data - data.mean()) / data.std())
    return data[z < threshold]

def data_processing(data):
    n_f = data[0].shape[1]
    final = np.zeros(n_f)

    for n in range(n_f):
        final[n] = outlier_filter(data[:,0,n]).mean()
    
    return final


if __name__ == "__main__":

    Yi = []
    Yf = []
    subprocess.run('make for_py', shell=True)
    for i in range(runs):
        subprocess.run('sudo taskset -c 5 ./client > /dev/null', shell=True)
        output_i = np.loadtxt('./scripts/iterative.txt', dtype='int').T
        output_f = np.loadtxt('./scripts/fast_doubling.txt', dtype='int').T
        Yi.append(np.delete(output_i, 0, 0))
        Yf.append(np.delete(output_f, 0, 0))

    X = output_i[0]

    Yi = np.array(Yi)
    Yi = data_processing(Yi)

    Yf = np.array(Yf)
    Yf = data_processing(Yf)

    fig, ax = plt.subplots(1, 1, sharey = True)
    ax.set_title('perf', fontsize = 16)
    ax.set_xlabel(r'$n_{th}$ fibonacci', fontsize = 16)
    ax.set_ylabel('time (ns)', fontsize = 16)

    ax.plot(X, Yi, marker = '+', markersize = 3, label = 'iterative')
    ax.plot(X, Yf, marker = '*', markersize = 3, label = 'fast_doubling')
    ax.legend(loc = 'upper left')

    plt.savefig('time.png')
    subprocess.run('sudo rmmod fibdrv || true >/dev/null', shell=True)
