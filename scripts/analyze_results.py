import pandas as pd
import sys

try:
    df = pd.read_csv('results_serial.csv')
    df = df.dropna(subset=['time_ms'])
    
    print("=== Benchmark Analysis ===")
    print(f"Total Runs: {len(df)}")
    print(f"Unique Graphs: {df['graph'].nunique()}")
    
    print("\n=== Results by Graph (Mean) ===")
    # Group by graph and take mean, include class (take first since it should be constant)
    summary = df.groupby('graph').agg({
        'class': 'first',
        'est_diameter': 'mean',
        'delta': 'mean',
        'time_ms': 'mean'
    }).sort_values('time_ms')
    
    pd.set_option('display.max_rows', None)
    print(summary)
    
    print("\n=== Classification Check ===")
    road_cnt = summary[summary['class'] == 'ROAD'].shape[0]
    social_cnt = summary[summary['class'] == 'SOCIAL'].shape[0]
    print(f"Total ROAD: {road_cnt}")
    print(f"Total SOCIAL: {social_cnt}")
    
except Exception as e:
    print(f"Analysis Error: {e}")
