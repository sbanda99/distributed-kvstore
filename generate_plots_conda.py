#!/usr/bin/env python3

# Generate plots for evaluation report using conda matplotlib.

import json
import sys

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Error: matplotlib not available. Please activate conda environment.")
    sys.exit(1)

def load_results():
    with open('results_data.json', 'r') as f:
        return json.load(f)

def generate_plot(results, servers_list, workload_key, plot_type, title_suffix, filename, allowed_clients=[20, 30, 40, 50, 60, 70, 80]):
    """Generate a single plot."""
    fig, axes = plt.subplots(1, len(servers_list), figsize=(6*len(servers_list), 5))
    if len(servers_list) == 1:
        axes = [axes]
    
    fig.suptitle(title_suffix, fontsize=16, fontweight='bold')
    
    for idx, servers in enumerate(servers_list):
        ax = axes[idx]
        server_data = results.get(str(servers), {})
        workload_data = server_data.get(workload_key, {})
        
        abd_data = workload_data.get('abd', {})
        blocking_data = workload_data.get('blocking', {})
        
        # Filter to only allowed clients and sort
        abd_clients = sorted([c for c in [int(k) for k in abd_data.keys()] if c in allowed_clients])
        blocking_clients = sorted([c for c in [int(k) for k in blocking_data.keys()] if c in allowed_clients])
        
        if plot_type == 'throughput':
            abd_values = [abd_data[str(c)].get('throughput', 0) for c in abd_clients]
            blocking_values = [blocking_data[str(c)].get('throughput', 0) for c in blocking_clients]
            ylabel = 'Throughput (ops/sec)'
            abd_label = 'ABD'
            blocking_label = 'Blocking'
            
            ax.plot(abd_clients, abd_values, 'o-', label='ABD', linewidth=2, markersize=6, color='#4A90E2')
            ax.plot(blocking_clients, blocking_values, 's-', label='Blocking', linewidth=2, markersize=6, color='#7ED321')
        elif plot_type == 'get_latency':
            abd_medians = [abd_data[str(c)].get('get_median', 0) / 1000.0 for c in abd_clients]
            abd_p95s = [abd_data[str(c)].get('get_p95', 0) / 1000.0 for c in abd_clients]
            blocking_medians = [blocking_data[str(c)].get('get_median', 0) / 1000.0 for c in blocking_clients]
            blocking_p95s = [blocking_data[str(c)].get('get_p95', 0) / 1000.0 for c in blocking_clients]
            ylabel = 'Latency (ms)'
            
            ax.plot(abd_clients, abd_medians, 'o-', label='ABD Median', linewidth=2, markersize=6, color='#4A90E2')
            ax.plot(abd_clients, abd_p95s, 'o--', label='ABD 95th', linewidth=2, markersize=6, color='#7BB3F0')
            ax.plot(blocking_clients, blocking_medians, 's-', label='Blocking Median', linewidth=2, markersize=6, color='#7ED321')
            ax.plot(blocking_clients, blocking_p95s, 's--', label='Blocking 95th', linewidth=2, markersize=6, color='#A5E85C')
        elif plot_type == 'put_latency':
            abd_medians = [abd_data[str(c)].get('put_median', 0) / 1000.0 for c in abd_clients]
            abd_p95s = [abd_data[str(c)].get('put_p95', 0) / 1000.0 for c in abd_clients]
            blocking_medians = [blocking_data[str(c)].get('put_median', 0) / 1000.0 for c in blocking_clients]
            blocking_p95s = [blocking_data[str(c)].get('put_p95', 0) / 1000.0 for c in blocking_clients]
            ylabel = 'Latency (ms)'
            
            ax.plot(abd_clients, abd_medians, 'o-', label='ABD Median', linewidth=2, markersize=6, color='#4A90E2')
            ax.plot(abd_clients, abd_p95s, 'o--', label='ABD 95th', linewidth=2, markersize=6, color='#7BB3F0')
            ax.plot(blocking_clients, blocking_medians, 's-', label='Blocking Median', linewidth=2, markersize=6, color='#7ED321')
            ax.plot(blocking_clients, blocking_p95s, 's--', label='Blocking 95th', linewidth=2, markersize=6, color='#A5E85C')
        
        ax.set_xlabel('Number of Clients', fontsize=12)
        ax.set_ylabel(ylabel, fontsize=12)
        ax.set_title(f'{servers} Server(s)', fontsize=14, fontweight='bold')
        ax.legend(fontsize=9)
        ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Generated: {filename}")

def main():
    if not HAS_MATPLOTLIB:
        return
    
    results = load_results()
    allowed_clients = [20, 30, 40, 50, 60, 70, 80]
    
    # Plot 1: Throughput for 90% GETs
    generate_plot(results, [1, 3, 5], '90%_GETs', 'throughput', 
                  'Throughput vs Number of Clients (90% GETs, 10% PUTs)',
                  'plot_throughput_90pct_gets.png', allowed_clients)
    
    # Plot 2: GET Latency for 90% GETs
    generate_plot(results, [1, 3, 5], '90%_GETs', 'get_latency',
                  'GET Latency vs Number of Clients (90% GETs, 10% PUTs)',
                  'plot_get_latency_90pct_gets.png', allowed_clients)
    
    # Plot 3: PUT Latency for 90% GETs
    generate_plot(results, [1, 3, 5], '90%_GETs', 'put_latency',
                  'PUT Latency vs Number of Clients (90% GETs, 10% PUTs)',
                  'plot_put_latency_90pct_gets.png', allowed_clients)
    
    # Plot 4: Throughput for 90% PUTs
    generate_plot(results, [1, 3, 5], '90%_PUTs', 'throughput',
                  'Throughput vs Number of Clients (10% GETs, 90% PUTs)',
                  'plot_throughput_90pct_puts.png', allowed_clients)
    
    # Plot 5: GET Latency for 90% PUTs
    generate_plot(results, [1, 3, 5], '90%_PUTs', 'get_latency',
                  'GET Latency vs Number of Clients (10% GETs, 90% PUTs)',
                  'plot_get_latency_90pct_puts.png', allowed_clients)
    
    # Plot 6: PUT Latency for 90% PUTs
    generate_plot(results, [1, 3, 5], '90%_PUTs', 'put_latency',
                  'PUT Latency vs Number of Clients (10% GETs, 90% PUTs)',
                  'plot_put_latency_90pct_puts.png', allowed_clients)
    
    print("\nAll 6 plots generated successfully!")

if __name__ == '__main__':
    main()

