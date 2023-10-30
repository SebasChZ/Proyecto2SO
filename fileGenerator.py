import os


def create_files():
    directory = "files"
    if not os.path.exists(directory):
        os.makedirs(directory)

    for i in range(1, 51):
        file_name = f"file{i}.txt"
        file_path = os.path.join(directory, file_name)
        with open(file_path, "w") as file:
            file.write(f"Proyecto 2 Sistemas operativos | Archivo empaquetado #: {i}")


if __name__ == "__main__":
    create_files()
