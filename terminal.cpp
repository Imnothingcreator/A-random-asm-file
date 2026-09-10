#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <pwd.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>

// ---------------------------------------------------------------------------
// ANSI color helpers
// ---------------------------------------------------------------------------
namespace col {
const char *RESET = "\033[0m";
const char *BOLD = "\033[1m";
const char *RED = "\033[31m";
const char *GREEN = "\033[32m";
const char *YELLOW = "\033[33m";
const char *BLUE = "\033[34m";
const char *MAGENTA = "\033[35m";
const char *CYAN = "\033[36m";
const char *WHITE = "\033[37m";
const char *BRIGHT_GREEN = "\033[92m";
const char *BRIGHT_CYAN = "\033[96m";
const char *BRIGHT_YELLOW = "\033[93m";
const char *BRIGHT_RED = "\033[91m";
const char *BRIGHT_BLUE = "\033[94m";
const char *BRIGHT_MAGENTA = "\033[95m";
const char *GRAY = "\033[90m";
} // namespace col

// ---------------------------------------------------------------------------
// Small utilities
// ---------------------------------------------------------------------------
static std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos)
    return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

static std::vector<std::string> split_ws(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream iss(s);
  std::string tok;
  while (iss >> tok)
    out.push_back(tok);
  return out;
}

static std::string human_bytes(unsigned long long kb) {
  double val = static_cast<double>(kb);
  const char *units[] = {"KiB", "MiB", "GiB", "TiB"};
  int u = 0;
  while (val >= 1024.0 && u < 3) {
    val /= 1024.0;
    u++;
  }
  std::ostringstream oss;
  oss.precision(2);
  oss << std::fixed << val << " " << units[u];
  return oss.str();
}

static std::string get_home() {
  const char *h = getenv("HOME");
  if (h)
    return h;
  struct passwd *pw = getpwuid(getuid());
  return pw ? pw->pw_dir : "/root";
}

static std::string state_file_path() { return get_home() + "/.cppterm_pkgs"; }

// ---------------------------------------------------------------------------
// Reads /etc/os-release for a pretty distro name
// ---------------------------------------------------------------------------
static std::string os_pretty_name() {
  std::ifstream f("/etc/os-release");
  std::string line;
  while (f && std::getline(f, line)) {
    if (line.rfind("PRETTY_NAME=", 0) == 0) {
      std::string v = line.substr(strlen("PRETTY_NAME="));
      if (!v.empty() && v.front() == '"')
        v.erase(0, 1);
      if (!v.empty() && v.back() == '"')
        v.pop_back();
      return v;
    }
  }
  struct utsname u{};
  uname(&u);
  return std::string(u.sysname) + " " + u.release;
}

static std::string read_cpu_model() {
  std::ifstream f("/proc/cpuinfo");
  std::string line;
  while (f && std::getline(f, line)) {
    if (line.rfind("model name", 0) == 0) {
      size_t c = line.find(':');
      if (c != std::string::npos)
        return trim(line.substr(c + 1));
    }
  }
  return "Unknown CPU";
}

static int count_cpus() {
  std::ifstream f("/proc/cpuinfo");
  std::string line;
  int n = 0;
  while (f && std::getline(f, line))
    if (line.rfind("processor", 0) == 0)
      n++;
  return n > 0 ? n : 1;
}

static std::string format_uptime(long seconds) {
  long days = seconds / 86400;
  long hours = (seconds % 86400) / 3600;
  long mins = (seconds % 3600) / 60;
  std::ostringstream oss;
  if (days > 0)
    oss << days << "d ";
  if (hours > 0 || days > 0)
    oss << hours << "h ";
  oss << mins << "m";
  return oss.str();
}

static std::string current_shell_name() {
  const char *sh = getenv("SHELL");
  if (!sh)
    return "cppterm";
  std::string s(sh);
  size_t p = s.find_last_of('/');
  return "cppterm (" + (p == std::string::npos ? s : s.substr(p + 1)) + ")";
}

// ---------------------------------------------------------------------------
// fastfetch-style ASCII logo (Custom "U" logo)
// ---------------------------------------------------------------------------
static std::vector<std::string> ascii_logo() {
  return {
      "   @@@@@@@        @@@@@@@",
      "   @@@@@@@        @@@@@@@",
      "   @@@@@@@        @@@@@@@",
      "   @@@@@@@        @@@@@@@",
      "   @@@@@@@        @@@@@@@",
      "   @@@@@@@        @@@@@@@",
      "   @@@@@@@        @@@@@@@",
      "   @@@@@@@        @@@@@@@",
      "   '@@@@@@        @@@@@@'",
      "    '@@@@@@@@@@@@@@@@@@' ",
      "      '@@@@@@@@@@@@@@'   ",
      "         '#@@@@@@#'      "
  };
}

// ---------------------------------------------------------------------------
// Built-in: fastfetch
// ---------------------------------------------------------------------------
static void cmd_fastfetch() {
  struct utsname u{};
  uname(&u);

  struct sysinfo si{};
  sysinfo(&si);

  char hostname[256] = {0};
  gethostname(hostname, sizeof(hostname) - 1);

  struct passwd *pw = getpwuid(getuid());
  std::string username = pw ? pw->pw_name : "user";

  std::string os = os_pretty_name();
  std::string kernel = std::string(u.sysname) + " " + u.release;
  std::string cpu = read_cpu_model();
  int ncpu = count_cpus();
  std::string uptime = format_uptime(si.uptime);

  unsigned long long totalram_kb =
      (unsigned long long)si.totalram * si.mem_unit / 1024ULL;
  unsigned long long freeram_kb =
      (unsigned long long)si.freeram * si.mem_unit / 1024ULL;
  unsigned long long usedram_kb = totalram_kb - freeram_kb;

  struct statvfs vfs{};
  std::string disk_line = "N/A";
  if (statvfs("/", &vfs) == 0) {
    unsigned long long total_kb =
        (unsigned long long)vfs.f_blocks * vfs.f_frsize / 1024ULL;
    unsigned long long free_kb =
        (unsigned long long)vfs.f_bfree * vfs.f_frsize / 1024ULL;
    unsigned long long used_kb = total_kb - free_kb;
    int pct = total_kb ? (int)((used_kb * 100) / total_kb) : 0;
    disk_line = human_bytes(used_kb) + " / " + human_bytes(total_kb) + " (" +
                std::to_string(pct) + "%)";
  }

  std::vector<std::pair<std::string, std::string>> rows;
  rows.push_back({"OS", os});
  rows.push_back({"Host", std::string(hostname)});
  rows.push_back({"Kernel", kernel});
  rows.push_back({"Uptime", uptime});
  rows.push_back({"Packages", [] {
                     std::ifstream f(state_file_path());
                     int n = 0;
                     std::string l;
                     while (std::getline(f, l))
                       if (!trim(l).empty())
                         n++;
                     return std::to_string(n) + " (cppterm-apt)";
                   }()});
  rows.push_back({"Shell", current_shell_name()});
  rows.push_back({"CPU", cpu + " (" + std::to_string(ncpu) + ")"});
  rows.push_back({"Memory", human_bytes(usedram_kb) + " / " +
                                 human_bytes(totalram_kb)});
  rows.push_back({"Disk (/)", disk_line});

  auto logo = ascii_logo();
  const char *logo_color = col::BRIGHT_CYAN;

  size_t max_lines = std::max(logo.size(), rows.size() + 2);
  std::string title_line =
      std::string(col::BOLD) + col::BRIGHT_GREEN + username + col::RESET +
      col::RED + "@" + col::RESET + col::BRIGHT_GREEN + hostname +
      col::RESET;
  size_t title_visible_len = username.size() + 1 + strlen(hostname);
  std::string sep_line(title_visible_len, '-');

  std::cout << "\n";
  for (size_t i = 0; i < max_lines; ++i) {
    std::string logo_part =
        i < logo.size() ? logo[i] : std::string(logo.empty() ? 0 : logo[0].size(), ' ');
    std::cout << logo_color << std::left;
    std::cout.width(0);
    std::cout << logo_part << col::RESET;

    // pad logo to consistent width
    int pad = 34 - (int)logo_part.size();
    if (pad > 0)
      std::cout << std::string(pad, ' ');
    else
      std::cout << "  ";

    if (i == 0) {
      std::cout << title_line;
    } else if (i == 1) {
      std::cout << col::GRAY << sep_line << col::RESET;
    } else {
      size_t ridx = i - 2;
      if (ridx < rows.size()) {
        std::cout << col::BOLD << col::BRIGHT_YELLOW << std::left;
        std::cout.width(0);
        std::cout << rows[ridx].first << col::RESET;
        int lp = 10 - (int)rows[ridx].first.size();
        if (lp > 0)
          std::cout << std::string(lp, ' ');
        std::cout << ": " << rows[ridx].second;
      }
    }
    std::cout << "\n";
  }

  // color swatches, like real fastfetch
  std::cout << "\n";
  int lpad = 34;
  std::cout << std::string(lpad, ' ');
  const char *swatches[] = {"\033[40m", "\033[41m", "\033[42m", "\033[43m",
                             "\033[44m", "\033[45m", "\033[46m", "\033[47m"};
  for (auto s : swatches)
    std::cout << s << "   " << col::RESET;
  std::cout << "\n\n";
}

// ---------------------------------------------------------------------------
// Built-in: simulated "sudo apt install <pkg> [<pkg2> ...]"
// This does NOT call the real apt or require root. It's a themed simulation
// that prints a believable install transcript and records state locally.
// ---------------------------------------------------------------------------
static std::vector<std::string> load_installed() {
  std::vector<std::string> pkgs;
  std::ifstream f(state_file_path());
  std::string line;
  while (std::getline(f, line)) {
    line = trim(line);
    if (!line.empty())
      pkgs.push_back(line);
  }
  return pkgs;
}

static void save_installed(const std::vector<std::string> &pkgs) {
  std::ofstream f(state_file_path(), std::ios::trunc);
  for (auto &p : pkgs)
    f << p << "\n";
}

static void progress_bar(const std::string &label, int width = 30) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> delay(8, 25);

  for (int i = 0; i <= width; ++i) {
    int pct = (i * 100) / width;
    std::cout << "\r" << label << " [" << col::BRIGHT_GREEN
               << std::string(i, '#') << col::RESET
               << std::string(width - i, ' ') << "] " << pct << "%"
               << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay(gen)));
  }
  std::cout << "\n";
}

static unsigned long long fake_size_for(const std::string &pkg) {
  // deterministic-ish pseudo size based on name hash, purely cosmetic
  std::hash<std::string> h;
  return 120 + (h(pkg) % 4000);
}

static void cmd_apt_install(const std::vector<std::string> &pkgs) {
  if (pkgs.empty()) {
    std::cout << col::BRIGHT_RED
               << "E: Must specify at least one package to install"
               << col::RESET << "\n";
    return;
  }

  auto installed = load_installed();

  std::vector<std::string> to_install;
  for (auto &p : pkgs) {
    if (std::find(installed.begin(), installed.end(), p) != installed.end()) {
      std::cout << p << " is already the newest version.\n";
    } else {
      to_install.push_back(p);
    }
  }

  if (to_install.empty()) {
    std::cout << "0 upgraded, 0 newly installed, 0 to remove.\n";
    return;
  }

  std::cout << "Reading package lists... Done\n";
  std::cout << "Building dependency tree... Done\n";
  std::cout << "Reading state information... Done\n";

  unsigned long long total_kb = 0;
  for (auto &p : to_install)
    total_kb += fake_size_for(p);

  std::cout << "The following NEW packages will be installed:\n  ";
  for (size_t i = 0; i < to_install.size(); ++i) {
    std::cout << col::BRIGHT_GREEN << to_install[i] << col::RESET;
    if (i + 1 < to_install.size())
      std::cout << " ";
  }
  std::cout << "\n";
  std::cout << "0 upgraded, " << to_install.size()
            << " newly installed, 0 to remove.\n";
  std::cout << "Need to get " << human_bytes(total_kb)
            << " of archives.\n";
  std::cout << "After this operation, " << human_bytes(total_kb * 3)
            << " of additional disk space will be used.\n";

  int idx = 1;
  for (auto &p : to_install) {
    unsigned long long sz = fake_size_for(p);
    std::cout << "Get:" << idx++ << " http://archive.ubuntu.com/ubuntu"
               << " noble/main amd64 " << p << " [" << human_bytes(sz)
               << "]\n";
  }
  std::cout << "Fetched " << human_bytes(total_kb) << " in 1s\n";

  for (auto &p : to_install) {
    progress_bar(std::string("Unpacking ") + p + "   ");
  }
  for (auto &p : to_install) {
    std::cout << "Setting up " << p << " ("
               << (1 + (fake_size_for(p) % 9)) << "." << (fake_size_for(p) % 20)
               << "-1) ...\n";
  }
  std::cout << "Processing triggers for man-db (2.12.0-4) ...\n";
  std::cout << "Processing triggers for libc-bin (2.39-0ubuntu8) ...\n";

  for (auto &p : to_install)
    installed.push_back(p);
  save_installed(installed);

  std::cout << col::BRIGHT_GREEN << "Done." << col::RESET << "\n";
}

static void cmd_apt_list_installed() {
  auto installed = load_installed();
  if (installed.empty()) {
    std::cout << "(no cppterm-apt packages installed yet — try: sudo apt "
                  "install <pkg>)\n";
    return;
  }
  std::cout << "Listing... Done\n";
  for (auto &p : installed) {
    unsigned long long sz = fake_size_for(p);
    std::cout << p << "/noble,now " << (1 + (sz % 9)) << "." << (sz % 20)
               << "-1 amd64 [installed]\n";
  }
}

static void cmd_apt_remove(const std::vector<std::string> &pkgs) {
  auto installed = load_installed();
  bool changed = false;
  for (auto &p : pkgs) {
    auto it = std::find(installed.begin(), installed.end(), p);
    if (it != installed.end()) {
      installed.erase(it);
      std::cout << "Removing " << p << " ... done\n";
      changed = true;
    } else {
      std::cout << "Package '" << p << "' is not installed, so not removed\n";
    }
  }
  if (changed)
    save_installed(installed);
}

// ---------------------------------------------------------------------------
// Real command execution fallback (fork/exec) — makes the shell actually
// usable for cd, ls, cat, grep, pipes via /bin/sh -c, etc.
// ---------------------------------------------------------------------------
static void run_external(const std::string &line) {
  pid_t pid = fork();
  if (pid < 0) {
    std::perror("fork");
    return;
  }
  if (pid == 0) {
    execl("/bin/sh", "sh", "-c", line.c_str(), (char *)nullptr);
    std::perror("exec");
    _exit(127);
  } else {
    int status = 0;
    waitpid(pid, &status, 0);
  }
}

static void print_help() {
  std::cout << col::BOLD << "cppterm — built-in commands\n" << col::RESET;
  std::cout << "  fastfetch                 Show system info panel\n";
  std::cout << "  sudo apt install <pkgs>   Simulated package install "
               "(no real root/apt used)\n";
  std::cout << "  sudo apt remove <pkgs>    Simulated package removal\n";
  std::cout << "  apt list --installed      List cppterm-apt 'installed' packages\n";
  std::cout << "  history                   Show command history\n";
  std::cout << "  clear                     Clear the screen\n";
  std::cout << "  help                      Show this help\n";
  std::cout << "  exit / quit               Leave cppterm\n";
  std::cout << "\nAnything else (cd, ls, cat, grep, pwd, echo, pipes, "
               "redirection, ...)\n"
               "is passed through to your real system shell (/bin/sh), so "
               "this behaves\nlike a genuine terminal for everyday use.\n";
}

int main() {
  std::vector<std::string> history;
  std::string home = get_home();
  std::string cwd_buf(4096, '\0');

  std::cout << col::BOLD << col::BRIGHT_CYAN
             << "cppterm — a C++ terminal (fastfetch + simulated apt "
                "built in)\n"
             << col::RESET
             << "Type 'help' for commands, 'fastfetch' for system info, "
                "'exit' to quit.\n";

  while (true) {
    char *cwd = getcwd(&cwd_buf[0], cwd_buf.size());
    std::string prompt_dir = cwd ? std::string(cwd) : "?";
    if (prompt_dir.rfind(home, 0) == 0)
      prompt_dir = "~" + prompt_dir.substr(home.size());

    struct passwd *pw = getpwuid(getuid());
    char hostname[256] = {0};
    gethostname(hostname, sizeof(hostname) - 1);
    std::string user = pw ? pw->pw_name : "user";

    std::cout << col::BRIGHT_GREEN << user << "@" << hostname << col::RESET
               << ":" << col::BRIGHT_BLUE << prompt_dir << col::RESET
               << "$ ";

    std::string line;
    if (!std::getline(std::cin, line)) {
      std::cout << "\n";
      break;
    }
    std::string trimmed = trim(line);
    if (trimmed.empty())
      continue;

    history.push_back(trimmed);
    auto tokens = split_ws(trimmed);

    if (tokens[0] == "exit" || tokens[0] == "quit") {
      break;
    } else if (tokens[0] == "help") {
      print_help();
    } else if (tokens[0] == "clear") {
      std::cout << "\033[2J\033[H";
    } else if (tokens[0] == "history") {
      for (size_t i = 0; i < history.size(); ++i)
        std::cout << "  " << (i + 1) << "  " << history[i] << "\n";
    } else if (tokens[0] == "fastfetch" || tokens[0] == "neofetch") {
      cmd_fastfetch();
    } else if (tokens[0] == "sudo" && tokens.size() >= 3 &&
               tokens[1] == "apt" &&
               (tokens[2] == "install" || tokens[2] == "remove")) {
      std::vector<std::string> pkgs(tokens.begin() + 3, tokens.end());
      // strip common apt flags for our simulation
      pkgs.erase(std::remove_if(pkgs.begin(), pkgs.end(),
                                 [](const std::string &s) {
                                   return !s.empty() && s[0] == '-';
                                 }),
                 pkgs.end());
      std::cout << "[sudo] password for " << user << ": "
                 << col::GRAY << "(skipped — simulated apt, no real root "
                    "needed)"
                 << col::RESET << "\n";
      if (tokens[2] == "install")
        cmd_apt_install(pkgs);
      else
        cmd_apt_remove(pkgs);
    } else if (tokens[0] == "apt" && tokens.size() >= 2 &&
               tokens[1] == "list") {
      cmd_apt_list_installed();
    } else if (tokens[0] == "apt" && tokens.size() >= 2 &&
               (tokens[1] == "install" || tokens[1] == "remove")) {
      std::cout << col::BRIGHT_YELLOW
                 << "Note: use 'sudo apt " << tokens[1]
                 << "' — this simulation expects sudo, just like real apt.\n"
                 << col::RESET;
    } else if (tokens[0] == "cd") {
      std::string target = tokens.size() > 1 ? tokens[1] : home;
      if (target == "~" || target.rfind("~/", 0) == 0)
        target = home + target.substr(1);
      if (chdir(target.c_str()) != 0)
        std::cout << "cd: " << target << ": No such file or directory\n";
    } else {
      run_external(trimmed);
    }
  }

  std::cout << "bye";
  return 0;
}
