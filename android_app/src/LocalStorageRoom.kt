package cl.inacap.ti3042.rastreador.data

import androidx.room.*

// Criterio 3.1.1.4 & 3.1.2.7: Almacenamiento temporal local (Off-line) y sincronización externa
@Entity(tableName = "telemetria_local")
data class TelemetriaEntity(
    @PrimaryKey(autoGenerate = true) val id: Int = 0,
    val deviceId: String,
    val latitud: Double,
    val longitud: Double,
    val velocidad: Float,
    val timestamp: Long,
    val sincronizado: Boolean = false
)

@Dao
interface TelemetriaDao {
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertarLectura(lectura: TelemetriaEntity)

    @Query("SELECT * FROM telemetria_local WHERE sincronizado = 0")
    suspend fun obtenerPendientesSincronizacion(): List<TelemetriaEntity>

    @Query("UPDATE telemetria_local SET sincronizado = 1 WHERE id = :id")
    suspend fun marcarComoSincronizado(id: Int)
}

@Database(entities = [TelemetriaEntity::class], version = 1)
abstract class AppDatabase : RoomDatabase() {
    abstract fun telemetriaDao(): TelemetriaDao
}
