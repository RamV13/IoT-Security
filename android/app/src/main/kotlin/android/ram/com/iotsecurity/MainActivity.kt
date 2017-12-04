package android.ram.com.iotsecurity

import android.app.Activity
import android.content.Context
import android.os.Bundle
import android.os.Looper
import org.jetbrains.anko.*
import org.jetbrains.anko.sdk25.coroutines.onClick
import kotlin.concurrent.thread

class MainActivity : Activity() {

    companion object {
        private val IP = "104.131.124.11"
        private val PORT = 3001
        var armed = false
        var sounding = false

        private fun send(context: Context, route: String) {
            thread {
                try {
                    khttp.get("http://$IP:$PORT/$route")
                } catch (e: Exception) {
                    Looper.prepare()
                    context.toast("server error")
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        verticalLayout {
            val mainContext = context
            val btn1 = toggleButton {
                textOff = "Disarmed"
                textOn = "Armed"
                onClick {
                    armed = !armed
                    send(mainContext, if (armed) "arm" else "disarm")
                }
            }.lparams(height = 750, width = matchParent)
            btn1.setText(R.string.toggle_one_off)

            val btn2 = toggleButton {
                textOff = "Silence"
                textOn = "Sounding"
                onClick {
                    sounding = !sounding
                    send(mainContext, if (sounding) "sound" else "disarm")
                }
            }.lparams(height = 750, width = matchParent)
            btn2.setText(R.string.toggle_two_off)
        }
    }

}
