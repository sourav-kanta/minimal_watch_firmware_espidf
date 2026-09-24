#!/usr/bin/env python3

import math
import os
import re
import sys

import numpy as np
import matplotlib.pyplot as plt


INPUT_FILE = "stage1_motion_test_vectors.txt"
OUTPUT_DIR = "stage1_motion_plots"

SAMPLE_SCALING_FACTOR = 0.0002441

GRAVITY_ALPHA = 0.1
VERTICAL_ACCEL_ALPHA = 0.2


def parse_vectors(filename):
    scenarios = {}

    current_name = None
    current_expected_count = None
    current_samples = []

    header_re = re.compile(r"^#\s+(\S+)\s+(\d+)\s*$")

    with open(filename, "r") as file:
        for line_number, line in enumerate(file, 1):
            line = line.strip()

            if not line:
                continue

            match = header_re.match(line)

            if match:
                if current_name is not None:
                    scenarios[current_name] = {
                        "expected_count": current_expected_count,
                        "samples": current_samples,
                    }

                current_name = match.group(1)
                current_expected_count = int(match.group(2))
                current_samples = []

                continue

            if line.startswith("#"):
                continue

            parts = line.split()

            if len(parts) != 3:
                raise ValueError(
                    f"{filename}:{line_number}: "
                    f"expected 3 integers, got: {line}"
                )

            try:
                x = int(parts[0])
                y = int(parts[1])
                z = int(parts[2])
            except ValueError as exc:
                raise ValueError(
                    f"{filename}:{line_number}: "
                    f"invalid integer sample: {line}"
                ) from exc

            if not (-32768 <= x <= 32767):
                raise ValueError(
                    f"{filename}:{line_number}: "
                    f"X outside int16_t range"
                )

            if not (-32768 <= y <= 32767):
                raise ValueError(
                    f"{filename}:{line_number}: "
                    f"Y outside int16_t range"
                )

            if not (-32768 <= z <= 32767):
                raise ValueError(
                    f"{filename}:{line_number}: "
                    f"Z outside int16_t range"
                )

            current_samples.append((x, y, z))

    if current_name is not None:
        scenarios[current_name] = {
            "expected_count": current_expected_count,
            "samples": current_samples,
        }

    for name, scenario in scenarios.items():
        actual = len(scenario["samples"])
        expected = scenario["expected_count"]

        if actual != expected:
            print(
                f"WARNING: {name}: "
                f"header says {expected} samples, "
                f"parsed {actual}"
            )

    return scenarios


def raw_to_g(samples):
    """
    Convert raw int16_t accelerometer values to g.
    """
    raw = np.asarray(samples, dtype=np.float32)
    return raw * SAMPLE_SCALING_FACTOR


def calculate_stage1_vertical_accel(samples):
    """
    Reproduce the relevant Stage 1 vertical acceleration processing.

    Production path:

        XYZ
          |
          v
        gravity tracking
          |
          v
        projection onto gravity
          |
          v
        vertical acceleration - 1g
          |
          v
        vertical acceleration smoothing
    """

    xyz = raw_to_g(samples)

    gravity = np.zeros(3, dtype=np.float32)

    accel_mod_raw = np.zeros(
        len(xyz),
        dtype=np.float32,
    )

    accel_mod_filtered = np.zeros(
        len(xyz),
        dtype=np.float32,
    )

    gravity_initialized = False

    for i, curr in enumerate(xyz):

        if not gravity_initialized:
            gravity[:] = curr
            gravity_initialized = True

        relative = curr - gravity

        gravity += GRAVITY_ALPHA * relative

        gravity_norm_sq = float(np.dot(gravity, gravity))

        if gravity_norm_sq > 1e-12:
            gravity_norm = math.sqrt(gravity_norm_sq)

            vertical_component = (
                float(np.dot(curr, gravity))
                / gravity_norm
            )

            vertical = vertical_component - 1.0
        else:
            vertical = 0.0

        accel_mod_raw[i] = vertical

        if i == 0:
            accel_mod_filtered[i] = vertical
        else:
            accel_mod_filtered[i] = (
                VERTICAL_ACCEL_ALPHA
                * accel_mod_filtered[i - 1]
                + (1.0 - VERTICAL_ACCEL_ALPHA)
                * vertical
            )

    return accel_mod_raw, accel_mod_filtered


def calculate_accel_magnitude(samples):
    """
    Calculate total acceleration magnitude in g.
    """

    xyz = raw_to_g(samples)

    return np.linalg.norm(xyz, axis=1)


def calculate_gravity(samples):
    """
    Calculate the magnitude of the Stage 1 tracked gravity vector.
    """

    xyz = raw_to_g(samples)

    gravity = np.zeros(3, dtype=np.float32)

    result = np.zeros(
        len(xyz),
        dtype=np.float32,
    )

    initialized = False

    for i, curr in enumerate(xyz):

        if not initialized:
            gravity[:] = curr
            initialized = True

        relative = curr - gravity

        gravity += GRAVITY_ALPHA * relative

        result[i] = np.linalg.norm(gravity)

    return result


def plot_scenario(name, scenario):
    samples = scenario["samples"]

    accel_mod_raw, accel_mod_filtered = (
        calculate_stage1_vertical_accel(samples)
    )

    accel_magnitude = calculate_accel_magnitude(
        samples
    )

    gravity_magnitude = calculate_gravity(
        samples
    )

    sample_index = np.arange(len(samples))

    fig, axes = plt.subplots(
        3,
        1,
        figsize=(14, 10),
        sharex=True,
    )

    fig.suptitle(
        f"Stage 1 motion fixture: {name}",
        fontsize=14,
    )

    # ------------------------------------------------------------
    # Raw XYZ acceleration
    # ------------------------------------------------------------

    xyz_g = raw_to_g(samples)

    axes[0].plot(
        sample_index,
        xyz_g[:, 0],
        label="X",
    )

    axes[0].plot(
        sample_index,
        xyz_g[:, 1],
        label="Y",
    )

    axes[0].plot(
        sample_index,
        xyz_g[:, 2],
        label="Z",
    )

    axes[0].set_ylabel("Acceleration (g)")
    axes[0].set_title("Raw physical acceleration")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()

    # ------------------------------------------------------------
    # Acceleration magnitude / tracked gravity
    # ------------------------------------------------------------

    axes[1].plot(
        sample_index,
        accel_magnitude,
        label="|accel|",
    )

    axes[1].plot(
        sample_index,
        gravity_magnitude,
        label="tracked |gravity|",
    )

    axes[1].axhline(
        1.0,
        linestyle="--",
        linewidth=1,
        label="1 g",
    )

    axes[1].set_ylabel("Magnitude (g)")
    axes[1].set_title(
        "Acceleration magnitude and tracked gravity"
    )
    axes[1].grid(True, alpha=0.3)
    axes[1].legend()

    # ------------------------------------------------------------
    # Actual Stage 1 vertical acceleration signal
    # ------------------------------------------------------------

    axes[2].plot(
        sample_index,
        accel_mod_raw,
        label="vertical accel",
        alpha=0.45,
    )

    axes[2].plot(
        sample_index,
        accel_mod_filtered,
        label="Stage 1 filtered",
        linewidth=2,
    )

    axes[2].axhline(
        0.0,
        linestyle="--",
        linewidth=1,
    )

    axes[2].axhline(
        0.07,
        linestyle=":",
        linewidth=1,
        label="peak prominence threshold",
    )

    axes[2].set_xlabel("Sample")
    axes[2].set_ylabel("Acceleration (g)")
    axes[2].set_title(
        "Stage 1 vertical acceleration"
    )
    axes[2].grid(True, alpha=0.3)
    axes[2].legend()

    plt.tight_layout()

    os.makedirs(
        OUTPUT_DIR,
        exist_ok=True,
    )

    output_path = os.path.join(
        OUTPUT_DIR,
        f"{name}.png",
    )

    fig.savefig(
        output_path,
        dpi=150,
        bbox_inches="tight",
    )

    print(f"Saved: {output_path}")

    return fig


def print_summary(name, scenario):
    samples = scenario["samples"]

    accel_mod_raw, accel_mod_filtered = (
        calculate_stage1_vertical_accel(samples)
    )

    magnitude = calculate_accel_magnitude(
        samples
    )

    print()
    print(f"=== {name} ===")
    print(f"Samples:       {len(samples)}")

    print(
        f"Accel |a|:     "
        f"{np.min(magnitude):.4f} .. "
        f"{np.max(magnitude):.4f} g"
    )

    print(
        f"Vertical raw:  "
        f"{np.min(accel_mod_raw):.4f} .. "
        f"{np.max(accel_mod_raw):.4f} g"
    )

    print(
        f"Vertical filt: "
        f"{np.min(accel_mod_filtered):.4f} .. "
        f"{np.max(accel_mod_filtered):.4f} g"
    )


def main():
    filename = (
        sys.argv[1]
        if len(sys.argv) > 1
        else INPUT_FILE
    )

    scenarios = parse_vectors(filename)

    if not scenarios:
        raise RuntimeError(
            f"No scenarios found in {filename}"
        )

    print(
        f"Parsed {len(scenarios)} scenarios "
        f"from {filename}"
    )

    for name, scenario in scenarios.items():
        print_summary(name, scenario)
        plot_scenario(name, scenario)

    print()
    print(f"Plots written to: {OUTPUT_DIR}/")
    print()
    print("Close the plot windows to exit.")

    plt.show()


if __name__ == "__main__":
    main()
