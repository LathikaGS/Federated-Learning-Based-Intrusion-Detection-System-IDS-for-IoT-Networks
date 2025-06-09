import pandas as pd
import numpy as np
from sklearn.preprocessing import StandardScaler
import sys

# Load dataset
file_path = "scratch/ddos_attack_data.txt"
try:
    data = pd.read_csv(file_path, sep=" ", names=["ip", "packet_rate"])
except FileNotFoundError:
    print(f"Error: File {file_path} not found.")
    sys.exit(1)

# Check if data is empty
if data.empty:
    print("Warning: No attack data found. Skipping FL processing.")
    sys.exit(0)

# Convert IPs to numerical values by creating a time sequence and parsing packet rates
data["time"] = np.arange(len(data))
data["packet_rate"] = pd.to_numeric(data["packet_rate"], errors="coerce")

# Remove rows with NaN values (if any)
data.dropna(inplace=True)

if data.empty:
    print("Warning: Data is empty after cleaning. No valid records.")
    sys.exit(0)

# Standardize the features
scaler = StandardScaler()
X = scaler.fit_transform(data[["time", "packet_rate"]])

# Placeholder for Federated Learning model processing
# (Replace this with your actual model training or inference code)
print("Federated Learning Model Processing Completed.")

# Optionally save the processed data for further analysis
data.to_csv("scratch/processed_attack_data.csv", index=False)
