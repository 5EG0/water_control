#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_LEVEL 0.0
#define MAX_LEVEL 100.0
#define LOW_LIMIT 20.0
#define HIGH_LIMIT 90.0

typedef enum {
    IDLE,
    FILLING,
    DRAINING,
    FULL,
    EMPTY
} SystemState;

typedef struct {
    float waterLevel;
    float inflowRate;
    float outflowRate;
    int pumpStatus;
    int valveStatus;
    int simulationStatus;
    SystemState state;
} WaterTank;

// Inisialisasi sistem
void initializeSystem(WaterTank *tank) {
    tank->waterLevel = 45.0;
    tank->inflowRate = 5.0;
    tank->outflowRate = 4.0;
    tank->pumpStatus = 0;
    tank->valveStatus = 0;
    tank->simulationStatus = 0;
    tank->state = IDLE;
}

// Kontrol otomatis pompa dengan hysteresis
void controlPump(WaterTank *tank) {
    if (tank->waterLevel <= LOW_LIMIT) {
        tank->pumpStatus = 1;
    }
    else if (tank->waterLevel >= HIGH_LIMIT) {
        tank->pumpStatus = 0;
    }
}

// Menghitung perubahan level air
void calculateWaterLevel(WaterTank *tank) {
    float inflow = 0;
    float outflow = 0;

    if (tank->pumpStatus == 1) {
        inflow = tank->inflowRate;
    }

    if (tank->valveStatus == 1) {
        outflow = tank->outflowRate;
    }

    tank->waterLevel += inflow - outflow;

    // Membatasi level air
    if (tank->waterLevel > MAX_LEVEL) {
        tank->waterLevel = MAX_LEVEL;
    }

    if (tank->waterLevel < MIN_LEVEL) {
        tank->waterLevel = MIN_LEVEL;
    }
}

// Menentukan kondisi sistem
void updateSystem(WaterTank *tank) {
    if (tank->waterLevel >= MAX_LEVEL) {
        tank->state = FULL;
    }
    else if (tank->waterLevel <= MIN_LEVEL) {
        tank->state = EMPTY;
    }
    else if (tank->pumpStatus == 1 && tank->valveStatus == 1) {
        tank->state = FILLING;
    }
    else if (tank->pumpStatus == 1) {
        tank->state = FILLING;
    }
    else if (tank->valveStatus == 1) {
        tank->state = DRAINING;
    }
    else {
        tank->state = IDLE;
    }
}

// Menampilkan status sistem
void displayStatus(WaterTank tank) {
    printf("\n========== WATER TANK ==========\n");
    printf("Water Level : %.1f%%\n", tank.waterLevel);
    printf("Pump        : %s\n",
           tank.pumpStatus ? "ON" : "OFF");
    printf("Drain Valve : %s\n",
           tank.valveStatus ? "OPEN" : "CLOSED");

    printf("System State: ");

    switch (tank.state) {
        case FILLING:  printf("FILLING\n"); break;
        case DRAINING: printf("DRAINING\n"); break;
        case FULL:     printf("FULL\n"); break;
        case EMPTY:    printf("EMPTY\n"); break;
        default:       printf("IDLE\n"); break;
    }

    printf("================================\n");
}

// Menyimpan riwayat ke TXT
void saveHistory(WaterTank tank) {
    FILE *file = fopen("history.txt", "a");

    if (file == NULL) {
        printf("Gagal membuka file!\n");
        return;
    }

    fprintf(file,
        "Level: %.1f%% | Pump: %s | Valve: %s\n",
        tank.waterLevel,
        tank.pumpStatus ? "ON" : "OFF",
        tank.valveStatus ? "OPEN" : "CLOSED"
    );

    fclose(file);
}

// Menu utama
void showMenu() {
    printf("\n===== WATER TANK CONTROL =====\n");
    printf("1. Start simulation\n");
    printf("2. Stop simulation\n");
    printf("3. Open drain valve\n");
    printf("4. Close drain valve\n");
    printf("5. Reset simulation\n");
    printf("6. Exit\n");
    printf("Choose: ");
}

// Satu langkah simulasi (dipakai tombol Start dan timer di web)
void stepSimulation(WaterTank *tank) {
    controlPump(tank);
    calculateWaterLevel(tank);
    updateSystem(tank);
}

// Menjalankan satu pilihan menu, mengembalikan pesan untuk pengguna.
// Dipakai bersama oleh mode menu (terminal) dan mode API (web).
const char *applyAction(WaterTank *tank, int choice) {
    switch (choice) {
        case 1:
            tank->simulationStatus = 1;
            stepSimulation(tank);
            return "Simulasi berjalan.";

        case 2:
            tank->simulationStatus = 0;
            tank->pumpStatus = 0;
            tank->state = IDLE;
            return "Simulasi dihentikan.";

        case 3:
            tank->valveStatus = 1;
            return "Katup dibuka.";

        case 4:
            tank->valveStatus = 0;
            return "Katup ditutup.";

        case 5:
            initializeSystem(tank);
            return "Simulasi di-reset.";

        case 6:
            return "Program selesai.";

        default:
            return "Pilihan tidak tersedia.";
    }
}

// ---------- Mode API untuk web ----------
// Pemakaian:
//   water_tank --api <aksi> [level pump valve sim state]
// aksi: start | stop | open | close | reset | tick
// Hasil: satu baris JSON di stdout.
static const char *STATE_NAMES[] = {"IDLE", "FILLING", "DRAINING", "FULL", "EMPTY"};

int runApi(int argc, char *argv[]) {
    WaterTank tank;
    const char *action = argc > 2 ? argv[2] : "";
    const char *message = "";

    initializeSystem(&tank);

    if (argc >= 8) {
        tank.waterLevel       = (float)atof(argv[3]);
        tank.pumpStatus       = atoi(argv[4]);
        tank.valveStatus      = atoi(argv[5]);
        tank.simulationStatus = atoi(argv[6]);
        int st = atoi(argv[7]);
        tank.state = (st >= IDLE && st <= EMPTY) ? (SystemState)st : IDLE;
    }

    if (strcmp(action, "start") == 0)      message = applyAction(&tank, 1);
    else if (strcmp(action, "stop") == 0)  message = applyAction(&tank, 2);
    else if (strcmp(action, "open") == 0)  message = applyAction(&tank, 3);
    else if (strcmp(action, "close") == 0) message = applyAction(&tank, 4);
    else if (strcmp(action, "reset") == 0) message = applyAction(&tank, 5);
    else if (strcmp(action, "tick") == 0) {
        if (tank.simulationStatus == 1) stepSimulation(&tank);
    } else {
        printf("{\"error\":\"Aksi tidak dikenal\"}\n");
        return 2;
    }

    printf("{\"waterLevel\":%.2f,\"inflowRate\":%.2f,\"outflowRate\":%.2f,"
           "\"pumpStatus\":%d,\"valveStatus\":%d,\"simulationStatus\":%d,"
           "\"state\":\"%s\",\"stateCode\":%d,\"message\":\"%s\"}\n",
           tank.waterLevel, tank.inflowRate, tank.outflowRate,
           tank.pumpStatus, tank.valveStatus, tank.simulationStatus,
           STATE_NAMES[tank.state], (int)tank.state, message);
    return 0;
}

int main(int argc, char *argv[]) {
    WaterTank tank;
    int choice;

    if (argc > 1 && strcmp(argv[1], "--api") == 0) {
        return runApi(argc, argv);
    }

    initializeSystem(&tank);

    do {
        displayStatus(tank);
        showMenu();

        if (scanf("%d", &choice) != 1) {
            printf("Input tidak valid!\n");
            return 1;
        }

        printf("%s\n", applyAction(&tank, choice));

        if (choice == 1) {
            saveHistory(tank);
        }

    } while (choice != 6);

    return 0;
}
