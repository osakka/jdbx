#!/usr/bin/env python3
"""
Generate HTML Performance Report

This script takes a JSON performance report from the JSON database
and generates an HTML report with visualizations.

Usage:
    python generate_html_report.py <performance_data.json> <output.html>
"""

import json
import sys
import os
from datetime import datetime

def generate_html_report(json_file, output_file):
    """Generate an HTML report from the JSON performance data"""
    
    # Read the template HTML file
    template_path = os.path.join(os.path.dirname(__file__), "performance_report_template.html")
    with open(template_path, 'r') as f:
        template = f.read()
    
    # Read the performance data
    with open(json_file, 'r') as f:
        performance_data = json.load(f)
    
    # Insert the performance data into the template
    html_content = template.replace('PERFORMANCE_DATA_PLACEHOLDER', json.dumps(performance_data))
    
    # Write the HTML report
    with open(output_file, 'w') as f:
        f.write(html_content)
    
    print(f"HTML report generated: {output_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <performance_data.json> <output.html>")
        sys.exit(1)
    
    json_file = sys.argv[1]
    output_file = sys.argv[2]
    
    generate_html_report(json_file, output_file)