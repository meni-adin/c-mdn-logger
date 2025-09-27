from pathlib import Path
import argparse
import json
import re
import subprocess
import utils

BUILD_TYPES = ['debug-sanitized', 'debug-nonsanitized', 'release-sanitized', 'release-nonsanitized']

def get_tests_executables_dict(build_type):
    command = f'ctest --preset test-{build_type} --show-only=json-v1'
    result = utils.run_command(command, shell=True, check=True, stdout=subprocess.PIPE, text=True)

    tests_json = json.loads(result.stdout)
    tests = tests_json.get('tests', [])
    tests_executables_dict = {}  # Using dictionary and not set as iterating it preserves insertion order
    for test in tests:
        command = test.get('command', [])
        if command:
            tests_executables_dict[command[0]] = None
        else:
            utils.colored_print(f"No commands found for test {test.get('name')}")
            exit(1)
    return tests_executables_dict

def run_tests(tests_executables_dict):
    for key in tests_executables_dict:
        command = f'{key}'
        if utils.running_on_macos():
            command = 'MallocNanoZone=0 ' + command
        utils.run_command(command, shell=True, check=True)

def run_memory_test(tests_executables_dict):
    if utils.program_available('valgrind'):
        command = f'valgrind --version'
        utils.run_command(command, shell=True, check=True)

        for key in tests_executables_dict:
            command = f'valgrind --error-exitcode=1 --leak-check=full {key}'
            utils.run_command(command, shell=True, check=True)

def run_clang_tidy_test(build_type):
    if utils.program_available('clang-tidy') and not (utils.running_on_linux() and utils.running_on_github_actions()):
        command = f'clang-tidy --version'
        utils.run_command(command, shell=True, check=True)

        compilation_database_path = utils.PROJECT_DIR/'build'/build_type/'compile_commands.json'
        script = utils.SCRIPTS_DIR/'run_clang_tidy.py'
        command = f'{utils.PYTHON_EXECUTABLE} {script} -d {compilation_database_path} -e'
        utils.run_command(command, shell=True, check=True)

def run_coverage_test(build_type):
    GCOV_RESULT_FILES_EXTENSION = 'gcda'

    if utils.running_on_windows():
        utils.colored_print('Skipping coverage test on Windows')
        return
    if not utils.program_available('gcov'):
        utils.colored_print("Skipping coverage test - 'gcov' not available")

    command = f'gcov --version'
    utils.run_command(command, shell=True, check=True)

    gcda_files = list(Path(f'{utils.BUILD_TOP_DIR/build_type/"src"}').rglob(f'*.{GCOV_RESULT_FILES_EXTENSION}'))
    utils.colored_print('gcda_files:')
    for gcda_file in gcda_files:
        utils.colored_print(f'{gcda_file}')
    if not gcda_files:
        raise FileNotFoundError(f"No '.{GCOV_RESULT_FILES_EXTENSION}' files found")

    for gcda_file in gcda_files:
        outputFileDir = Path(gcda_file).parent
        command = f'gcov {gcda_file}'
        result = utils.run_command(command, shell=True, cwd=outputFileDir, capture_output=True, text=True)
        if result.returncode != 0:
            utils.colored_print(result.stderr)
            raise subprocess.CalledProcessError(result.returncode, command)

        pattern = r'Lines executed:(\d+\.\d+)%'
        lines = result.stdout.splitlines()
        match = re.search(pattern, lines[1])
        if match:
            percentage = float(match.group(1))
        else:
            raise ValueError(f"Pattern '{pattern}' not found in gcov output")

        if int(percentage) == 100:
            color = utils.COLOR_GREEN
        elif percentage >= 80:
            color = utils.COLOR_YELLOW
        else:
            color = utils.COLOR_RED
        lines[1] = color + lines[1] + utils.COLOR_RESET

        utils.colored_print('\n'.join(lines))


def main():
    parser = argparse.ArgumentParser(description='Build and test C/C++ code')
    parser.add_argument('-b', '--build-type', type=str, choices=BUILD_TYPES, help='Type of the build', default=None)
    args = parser.parse_args()

    if args.build_type:
        requested_build_types = [args.build_type]
    else:
        requested_build_types = BUILD_TYPES

    utils.colored_print(f'{requested_build_types=:}\n')

    for build_type in requested_build_types:
        utils.colored_print(f'Current {build_type=:}\n')

        command = f'cmake --preset config-{build_type}'
        utils.run_command(command, shell=True, check=True)

        command = f'cmake --build --preset build-{build_type}'
        utils.run_command(command, shell=True, check=True)

        tests_executables_dict = get_tests_executables_dict(build_type)

        run_tests(tests_executables_dict)

        if 'nonsanitized' in build_type:
            run_memory_test(tests_executables_dict)
            run_clang_tidy_test(build_type)

        if 'debug' in build_type:
            run_coverage_test(build_type)

    utils.colored_print('\nBuild-and-test done successfully', color=utils.COLOR_GREEN)

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        utils.colored_print('An error occurred:', color=utils.COLOR_RED)
        raise e
