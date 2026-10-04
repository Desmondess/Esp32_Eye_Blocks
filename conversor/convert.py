#!/usr/bin/env python3

import sys
import struct
import subprocess
import tempfile
from pathlib import Path

# É pra ser o Mesmo valor usado no ESP32
#BLOCK_MAGIC = seja lá o que o magic for

# Estrutura C++:
#
# struct BlockHeader
# {
#     uint32_t magic;
#     uint32_t version;
#     uint32_t frameCount;
#     uint32_t duration;
#     uint32_t prevHash;
# };
#
# 5 x uint32_t = 20 bytes
BLOCK_HEADER_FORMAT = "<5I" #significa uint32_t
BLOCK_HEADER_SIZE = struct.calcsize(BLOCK_HEADER_FORMAT)


# struct FrameHeader
# {
#     uint32_t timestamp;
#     uint32_t size;
# };
#
# 2 x uint32_t = 8 bytes
FRAME_HEADER_FORMAT = "<2I"
FRAME_HEADER_SIZE = struct.calcsize(FRAME_HEADER_FORMAT)


# ============================================================
# Leitura de um bloco
# ============================================================

def read_block(filename):
    
    #Lê um arquivo .bin e retorna:
    #    [(timestamp_ms, jpeg_bytes), ...]
    

    frames = []

    with open(filename, "rb") as f:

        # Cabeçalho do bloco

        data = f.read(BLOCK_HEADER_SIZE)

        #verifica s o tamanho do bloco é o mesmo do tamanho no cabeçalho
        if len(data) != BLOCK_HEADER_SIZE:
            raise ValueError(
                f"{filename}: cabeçalho incompleto"
            )

        magic, version, frame_count, duration, prev_hash = \
            struct.unpack(BLOCK_HEADER_FORMAT, data)

        print(
            f"{filename.name}: "
            f"version={version}, "
            f"frames={frame_count}, "
            f"duration={duration} ms"
        )

        # Frames

        for frame_index in range(frame_count):

            header = f.read(FRAME_HEADER_SIZE)

            if len(header) != FRAME_HEADER_SIZE:
                print(
                    f"  AVISO: frame {frame_index} "
                    f"possui cabeçalho incompleto"
                )
                break

            timestamp, jpeg_size = struct.unpack(
                FRAME_HEADER_FORMAT,
                header
            )

            # Segurança contra arquivo corrompido
            if jpeg_size == 0:
                print(
                    f"  AVISO: frame {frame_index} "
                    f"possui tamanho zero"
                )
                continue

            # Lê JPEG

            jpeg_data = f.read(jpeg_size)

            if len(jpeg_data) != jpeg_size:
                print(
                    f"  AVISO: frame {frame_index} "
                    f"JPEG incompleto"
                )
                break

            # Verifica assinatura JPEG
            #Honestamente eu não lembro do que isso é, e acho que só apareceu depois da IA revisar o código.
            if not jpeg_data.startswith(b"\xFF\xD8"):
                print(
                    f"  AVISO: frame {frame_index} "
                    f"não parece ser um JPEG válido"
                )
                continue

            frames.append((timestamp, jpeg_data))

    return frames


# Extrai todos os blocos

def extract_frames(input_dir, output_dir):

    input_dir = Path(input_dir)
    output_dir = Path(output_dir)

    output_dir.mkdir(
        parents=True,
        exist_ok=True
    )

    # Ordena os blocos pelo nome
    block_files = sorted(
        input_dir.glob("*.bin")
    )

    if not block_files:
        raise RuntimeError(
            f"Nenhum arquivo .bin encontrado em {input_dir}"
        )

    all_frames = []

    # Lê todos os blocos

    for block_file in block_files:

        try:
            frames = read_block(block_file)

            all_frames.extend(frames)

        except Exception as e:

            print(
                f"ERRO lendo {block_file}: {e}"
            )

    if not all_frames:
        raise RuntimeError(
            "Nenhum frame encontrado."
        )

    # Ordena por timestamp

    #lembrar que o x[0] é o timestamp e x[1] é a imagem.
    all_frames.sort(
        key=lambda x: x[0]
    )

    # Detecta overflow de millis()
    # Para gravações normais isso não é necessário.
    # De novo, não lembro disso, deve ter surgido depois da revisão de IA.

    print()
    print(f"Total de frames: {len(all_frames)}")

    first_timestamp = all_frames[0][0]

    # Salva JPEGs

    for index, (timestamp, jpeg_data) in enumerate(all_frames):

        filename = output_dir / f"frame_{index:08d}.jpg"

        with open(filename, "wb") as f:
            f.write(jpeg_data)

    return all_frames


# Geração do arquivo de concatenação do FFmpeg
def create_concat_file(frames, frames_dir, concat_filename):

    frames_dir = Path(frames_dir)

    first_timestamp = frames[0][0]

    with open(concat_filename, "w", encoding="utf-8") as f:

        for index in range(len(frames)):

            timestamp = frames[index][0]

            # Tempo relativo ao primeiro frame
            current_time = (
                timestamp - first_timestamp
            ) / 1000.0

            filename = (
                frames_dir /
                f"frame_{index:08d}.jpg"
            )

            # FFmpeg concat demuxer precisa de caminhos
            # com aspas simples.
            filename_str = str(
                filename.resolve()
            ).replace("'", "'\\''")

            f.write(
                f"file '{filename_str}'\n"
            )

            # Duração até o próximo frame

            if index + 1 < len(frames):

                next_timestamp = frames[index + 1][0]

                duration = (
                    next_timestamp - timestamp
                ) / 1000.0

                # Evita duração zero/negativa
                if duration <= 0:
                    duration = 0.001

                f.write(
                    f"duration {duration:.6f}\n"
                )

            else:
                # Último frame.
                # Repete aproximadamente a duração
                # do frame anterior.
                if len(frames) >= 2:

                    duration = (
                        frames[-1][0] -
                        frames[-2][0]
                    ) / 1000.0

                    if duration <= 0:
                        duration = 0.001

                else:
                    duration = 0.033333

                f.write(
                    f"duration {duration:.6f}\n"
                )

        # O concat demuxer usa a última duração somente
        # quando existe uma entrada seguinte.
        #
        # Repetimos o último arquivo para garantir que
        # o último frame permaneça pelo tempo correto.
        if frames:

            last_filename = (
                frames_dir /
                f"frame_{len(frames)-1:08d}.jpg"
            )

            filename_str = str(
                last_filename.resolve()
            ).replace("'", "'\\''")

            f.write(
                f"file '{filename_str}'\n"
            )

# Gera MKV usando FFmpeg

def create_mkv(concat_file, output_file):

    command = [
        "ffmpeg",

        "-y",

        # concat demuxer
        "-f", "concat",

        # Permite caminhos absolutos
        "-safe", "0",

        "-i", str(concat_file),

        # Mantém JPEG/MJPEG
        "-c:v", "mjpeg",

        # Pixel format comum
        "-pix_fmt", "yuvj420p",

        # Contêiner Matroska
        "-f", "matroska",

        str(output_file)
    ]

    print()
    print("Executando FFmpeg:")
    print(" ".join(command))
    print()

    result = subprocess.run(command)

    if result.returncode != 0:

        raise RuntimeError(
            "FFmpeg retornou erro."
        )


# Main

def main():

    if len(sys.argv) != 3:

        print(
            "Uso:\n"
            "  python convert.py <pasta_blocos> <arquivo.mkv>\n\n"
            "Exemplo:\n"
            "  python convert.py ./blocos video.mkv"
        )

        sys.exit(1)

    input_dir = Path(sys.argv[1])
    output_file = Path(sys.argv[2])

    if not input_dir.exists():

        print(
            f"Erro: pasta não encontrada: {input_dir}"
        )

        sys.exit(1)

    # Pasta temporária para os JPEGs

    with tempfile.TemporaryDirectory(
        prefix="mkv_frames_"
    ) as temp_dir:

        frames_dir = Path(temp_dir)

        print(
            "Lendo blocos..."
        )

        frames = extract_frames(
            input_dir,
            frames_dir
        )

        concat_file = (
            frames_dir /
            "frames.txt"
        )

        print(
            "Criando lista de frames..."
        )

        create_concat_file(
            frames,
            frames_dir,
            concat_file
        )

        print(
            "Gerando MKV..."
        )

        create_mkv(
            concat_file,
            output_file
        )

    print()
    print(
        f"Concluído: {output_file}"
    )


if __name__ == "__main__":
    main()

#Como usar

#Instale o FFmpeg no computador e confirme que:

#ffmpeg -version


#funciona no terminal.

#Depois organize os arquivos:

#projeto/
#├── convert.py
#└── blocos/
#    ├── bloco_000.bin
#    ├── bloco_001.bin
#    ├── bloco_002.bin
#    └── ...


#Executar: python convert.py ./blocos video.mkv