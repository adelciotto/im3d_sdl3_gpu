// -- Defer -------------------------------------------------------------------

template<typename F> struct Priv_Defer {
  F f;
  Priv_Defer(F f) : f(f) {
  }
  ~Priv_Defer() {
    f();
  }
};

template<typename F> Priv_Defer<F> defer_func(F f) {
  return Priv_Defer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&]() { code; })

// -- Storage -----------------------------------------------------------------

template<typename Container>
bool read_storage_file(SDL_Storage* storage, const char* file_path, Container* out_container) {
  Uint64 file_size = 0;
  if (!SDL_GetStorageFileSize(storage, file_path, &file_size)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to get storage file size: %s",
        SDL_GetError());
    return false;
  }
  if (file_size == 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get storage file size: file size is 0");
    return false;
  }

  out_container->resize(file_size);
  if (!SDL_ReadStorageFile(storage, file_path, out_container->data(), file_size)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read storage file: %s", SDL_GetError());
    return false;
  }

  return true;
}
