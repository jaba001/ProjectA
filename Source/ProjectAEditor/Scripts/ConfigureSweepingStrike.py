import unreal


# This retired authoring entry point preserves files and assets when invoked directly.
# 폐기된 작성 도구를 직접 실행해도 파일과 에셋을 변경하지 않습니다.
if __name__ == "__main__":
    unreal.log_warning("ConfigureSweepingStrike.py is retired because Sweeping Strike was removed; no files or assets were changed")
