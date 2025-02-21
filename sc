echo "Format changed C++ files" | tee -a log \
&& git diff HEAD --name-only | grep -E '\.(h$)|(hpp$)|(cpp$)' | xargs clang-format -i

echo 'Format C++ files' | tee -a log \
&& find include src test -type f | grep -E '\.(h$)|(hpp$)|(cpp$)' | xargs clang-format -i
