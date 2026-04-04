package ai.human.app

import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import ai.human.app.data.readGatewayUrl
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

/**
 * Entry point for Google Assistant App Actions.
 * Receives queries from Assistant, forwards to the Human gateway,
 * and returns the response.
 */
class AssistantActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(intent: Intent) {
        val query = intent.getStringExtra("query")
            ?: intent.data?.getQueryParameter("query")
            ?: run {
                finish()
                return
            }

        CoroutineScope(Dispatchers.Main).launch {
            val client = HumanGatewayClient.getInstance(applicationContext)
            try {
                val url = this@AssistantActivity.readGatewayUrl()
                val response = client.sendMessage(url, query)
                Toast.makeText(this@AssistantActivity, response, Toast.LENGTH_LONG).show()
                val mainIntent = Intent(this@AssistantActivity, MainActivity::class.java).apply {
                    putExtra("assistant_response", response)
                    putExtra("assistant_query", query)
                    flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
                }
                startActivity(mainIntent)
            } catch (e: Exception) {
                Toast.makeText(
                    this@AssistantActivity,
                    "Failed to reach Human: ${e.localizedMessage}",
                    Toast.LENGTH_SHORT,
                ).show()
            } finally {
                client.disconnect()
            }
            finish()
        }
    }
}

/** Type bridge for assistant integrations; each call uses a fresh [GatewayClient] instance. */
object HumanGatewayClient {
    fun getInstance(@Suppress("UNUSED_PARAMETER") context: android.content.Context): GatewayClient =
        GatewayClient()
}
