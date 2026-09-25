from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt

base_dir = Path(__file__).resolve().parent

data_htm = pd.read_csv(base_dir/"HTM.csv")
data_qtm = pd.read_csv(base_dir/"QTM.csv")

fig, ax = plt.subplots(1, 2, figsize=(14, 5), sharey=True)

bars_htm = ax[0].bar(
    data_htm["depth"],
    data_htm["states"]
)

ax[0].set_xlabel("HTM Distance")
ax[0].set_ylabel("Number of States")
ax[0].set_title("2×2×2 Cube State Distribution under HTM")
ax[0].set_xticks(data_htm["depth"])

ax[0].bar_label(
    bars_htm,
    fmt="%d",
    padding=3,
    fontsize=8,
)

bars_qtm = ax[1].bar(
    data_qtm["depth"],
    data_qtm["states"]
)

ax[1].set_xlabel("QTM Distance")
ax[1].set_title("2×2×2 Cube State Distribution under QTM")
ax[1].set_xticks(data_qtm["depth"])

ax[1].bar_label(
    bars_qtm,
    fmt="%d",
    padding=3,
    fontsize=8,
)

plt.tight_layout()
plt.show()