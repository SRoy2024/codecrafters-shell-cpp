#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
#include <fstream>
#include <fcntl.h>
#include <iomanip>
#include <algorithm>

struct BackgroundJob {
    int id;
    pid_t pid;
    std::string command;
    std::string status;
    bool already_reported_done; 
};

void poll_background_jobs(std::vector<BackgroundJob>& bg_jobs) {
    for (auto& job : bg_jobs)
    {
        if (job.status == "Running")
        {
            int status;
            pid_t res = waitpid(job.pid, &status, WNOHANG);
            if (res > 0 && WIFEXITED(status))
            {
                job.status = "Done";
                if (job.command.size() >= 2 && job.command.substr(job.command.size() - 2) == " &")
                {
                    job.command = job.command.substr(0, job.command.size() - 2);
                }
            }
        }
    }
}

void print_jobs(std::vector<BackgroundJob>& bg_jobs, std::ostream* out, bool only_done) {
    int num_jobs = static_cast<int>(bg_jobs.size());
    for (int i = 0; i < num_jobs; ++i)
    {
        bool should_print = !only_done || (bg_jobs[i].status == "Done" && !bg_jobs[i].already_reported_done);
        
        if (should_print)
        {
            char marker = ' ';
            if (i == num_jobs - 1) marker = '+'; 
            else if (i == num_jobs - 2) marker = '-'; 

            (*out) << "[" << bg_jobs[i].id << "]" << marker << "  " 
                   << std::left << std::setw(24) << bg_jobs[i].status 
                   << bg_jobs[i].command << std::endl;

            if (bg_jobs[i].status == "Done")
            {
                bg_jobs[i].already_reported_done = true;
            }
        }
    }
}

void clean_done_jobs(std::vector<BackgroundJob>& bg_jobs) {
    std::vector<BackgroundJob> persistent_jobs;
    for (const auto& job : bg_jobs)
    {
        if (!job.already_reported_done) persistent_jobs.push_back(job);
    }
    bg_jobs = persistent_jobs;
}

bool is_builtin(const std::string& cmd) {
    return (cmd == "echo" || cmd == "exit" || cmd == "type" || cmd == "pwd" || cmd == "cd" || cmd == "jobs");
}

// Helper to execute builtins uniformly in parent or child environments
void execute_builtin(const std::vector<std::string>& args, std::ostream& out, std::ostream& err, std::vector<BackgroundJob>& bg_jobs) {
    if (args[0] == "exit") {
        exit(0); 
    }
    else if (args[0] == "echo") {
        for (size_t i = 1; i < args.size(); i++) {
            if (i > 1) out << " ";
            out << args[i];
        }
        out << std::endl;
    }
    else if (args[0] == "type") {
        if (args.size() < 2) return;
        std::string cmd = args[1];
        if (is_builtin(cmd)) {
            out << cmd << " is a shell builtin" << std::endl;
        } else {
            const char* pathEnv = std::getenv("PATH");
            std::string path = pathEnv ? pathEnv : "";
            std::stringstream ss(path);
            std::string dir;
            bool found = false;

            while (std::getline(ss, dir, ':')) {
                std::string fullPath = dir + "/" + cmd;
                if (access(fullPath.c_str(), X_OK) == 0) {
                    out << cmd << " is " << fullPath << std::endl;
                    found = true;
                    break;
                }
            }
            if (!found) err << cmd << ": not found" << std::endl;
        }
    }
    else if (args[0] == "pwd") {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) out << cwd << std::endl;
    }
    else if (args[0] == "cd") {
        if (args.size() < 2) return;
        std::string path = args[1];
        if (path == "~") path = getenv("HOME");
        if (chdir(path.c_str()) != 0) {
            err << "cd: " << path << ": No such file or directory" << std::endl;
        }
    }
    else if (args[0] == "jobs") {
        poll_background_jobs(bg_jobs);
        print_jobs(bg_jobs, &out, false); 
        clean_done_jobs(bg_jobs);
    }
}

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::vector<BackgroundJob> bg_jobs; 

    while (true)
    {
        poll_background_jobs(bg_jobs);
        print_jobs(bg_jobs, &std::cout, true);
        clean_done_jobs(bg_jobs);

        bool escaped = false;
        std::cout << "$ ";

        std::string command;
        if (!std::getline(std::cin, command)) break;

        std::vector<std::string> args;
        std::string current;
        bool in_single_quote = false, in_double_quote = false;

        for (char c : command)
        {
            if (escaped) {
                if (in_double_quote) {
                    if (c == '"' || c == '\\') current += c;
                    else { current += '\\'; current += c; }
                }
                else current += c;
                escaped = false;
            }
            else if (in_double_quote && c == '\\') escaped = true;
            else if (!in_single_quote && !in_double_quote && c == '\\') escaped = true;
            else if (c == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
            else if (c == '"' && !in_single_quote) in_double_quote = !in_double_quote;
            else if (c == ' ' && !in_single_quote && !in_double_quote) {
                if (!current.empty()) { args.push_back(current); current.clear(); }
            }
            else current += c;
        }
        if (!current.empty()) args.push_back(current);
        if (args.empty()) continue;

        bool is_background = false;
        if (args.back() == "&") {
            is_background = true;
            args.pop_back(); 
        }
        if (args.empty()) continue;

        // Pipeline Logic Execution
        auto pipe_it = std::find(args.begin(), args.end(), "|");
        if (pipe_it != args.end())
        {
            std::vector<std::string> left_args(args.begin(), pipe_it);
            std::vector<std::string> right_args(pipe_it + 1, args.end());

            if (left_args.empty() || right_args.empty()) continue;

            int pipe_fds[2];
            if (pipe(pipe_fds) == -1) continue;

            pid_t left_pid = fork();
            if (left_pid == 0)
            {
                dup2(pipe_fds[1], STDOUT_FILENO);
                close(pipe_fds[0]); close(pipe_fds[1]);

                if (is_builtin(left_args[0])) {
                    execute_builtin(left_args, std::cout, std::cerr, bg_jobs);
                    exit(0);
                } else {
                    std::vector<char*> left_argv;
                    for (auto& arg : left_args) left_argv.push_back(const_cast<char*>(arg.c_str()));
                    left_argv.push_back(nullptr);
                    execvp(left_argv[0], left_argv.data());
                    std::cerr << left_args[0] << ": not found" << std::endl;
                    exit(1);
                }
            }

            pid_t right_pid = fork();
            if (right_pid == 0)
            {
                dup2(pipe_fds[0], STDIN_FILENO);
                close(pipe_fds[0]); close(pipe_fds[1]);

                if (is_builtin(right_args[0])) {
                    execute_builtin(right_args, std::cout, std::cerr, bg_jobs);
                    exit(0);
                } else {
                    std::vector<char*> right_argv;
                    for (auto& arg : right_args) right_argv.push_back(const_cast<char*>(arg.c_str()));
                    right_argv.push_back(nullptr);
                    execvp(right_argv[0], right_argv.data());
                    std::cerr << right_args[0] << ": not found" << std::endl;
                    exit(1);
                }
            }

            close(pipe_fds[0]); close(pipe_fds[1]);

            if (is_background) {
                int next_job_id = 1;
                if (!bg_jobs.empty()) {
                    int max_id = 0;
                    for (const auto& job : bg_jobs) if (job.id > max_id) max_id = job.id;
                    next_job_id = max_id + 1;
                }
                std::cout << "[" << next_job_id << "] " << right_pid << std::endl;
                bg_jobs.push_back({next_job_id, right_pid, command, "Running", false});
            } else {
                waitpid(left_pid, nullptr, 0);
                waitpid(right_pid, nullptr, 0);
            }
            continue;
        }

        // Non-pipelined Logic Execution
        std::string outputFile, errorFile;
        bool redirectStdout = false, redirectStderr = false;
        bool appendStdout = false, appendStderr = false;

        for (size_t i = 0; i + 1 < args.size();) {
            if (args[i] == ">" || args[i] == "1>") {
                redirectStdout = true; appendStdout = false; outputFile = args[i + 1];
                args.erase(args.begin() + i, args.begin() + i + 2);
            } else if (args[i] == ">>" || args[i] == "1>>") {
                redirectStdout = true; appendStdout = true; outputFile = args[i + 1];
                args.erase(args.begin() + i, args.begin() + i + 2);
            } else if (args[i] == "2>") {
                redirectStderr = true; appendStderr = false; errorFile = args[i + 1];
                args.erase(args.begin() + i, args.begin() + i + 2);
            } else if (args[i] == "2>>") {
                redirectStderr = true; appendStderr = true; errorFile = args[i + 1];
                args.erase(args.begin() + i, args.begin() + i + 2);
            } else ++i;
        }

        std::ofstream outFile; std::ostream* out = &std::cout;
        if (redirectStdout) {
            if (appendStdout) outFile.open(outputFile, std::ios::app);
            else outFile.open(outputFile);
            out = &outFile;
        }
        std::ofstream errFile; std::ostream* err = &std::cerr;
        if (redirectStderr) {
            if (appendStderr) errFile.open(errorFile, std::ios::app);
            else errFile.open(errorFile);
            err = &errFile;
        }

        if (args.empty()) continue;

        if (is_builtin(args[0])) {
            // Run builtins in the current shell process context if no pipe is involved
            execute_builtin(args, *out, *err, bg_jobs);
        } else {
            std::string full_command_str = "";
            for (size_t i = 0; i < args.size(); ++i) {
                full_command_str += args[i];
                if (i + 1 < args.size()) full_command_str += " ";
            }
            if (is_background) full_command_str += " &";

            std::vector<char*> argv;
            for (auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
            argv.push_back(nullptr);

            pid_t pid = fork();
            if (pid == 0) {
                if (redirectStdout) {
                    int fd = open(outputFile.c_str(), O_WRONLY | O_CREAT | (appendStdout ? O_APPEND : O_TRUNC), 0644);
                    dup2(fd, STDOUT_FILENO); close(fd);
                }
                if (redirectStderr) {
                    int fd = open(errorFile.c_str(), O_WRONLY | O_CREAT | (appendStderr ? O_APPEND : O_TRUNC), 0644);
                    dup2(fd, STDERR_FILENO); close(fd);
                }
                execvp(argv[0], argv.data());
                std::cerr << args[0] << ": not found" << std::endl;
                exit(1);
            }

            if (is_background) {
                int next_job_id = 1;
                if (!bg_jobs.empty()) {
                    int max_id = 0;
                    for (const auto& job : bg_jobs) if (job.id > max_id) max_id = job.id;
                    next_job_id = max_id + 1;
                }
                std::cout << "[" << next_job_id << "] " << pid << std::endl;
                bg_jobs.push_back({next_job_id, pid, full_command_str, "Running", false});
            } else waitpid(pid, nullptr, 0);
        }
    }
    return 0;
}