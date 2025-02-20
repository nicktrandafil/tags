echo "Format changed cpp files" | tee -a log \
&& git diff HEAD --name-only | grep -E '\.(h$)|(hpp$)|(cpp$)' | xargs clang-format -i
