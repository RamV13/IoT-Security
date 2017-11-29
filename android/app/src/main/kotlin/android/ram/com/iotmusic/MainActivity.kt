package android.ram.com.iotmusic

import android.app.Activity
import android.content.Context
import android.os.Bundle
import android.os.Looper
import org.jetbrains.anko.*
import org.jetbrains.anko.sdk25.coroutines.onClick
import kotlin.concurrent.thread

class MainActivity : Activity() {

    companion object {
        // private val IP = "104.131.124.11"
        private val IP = "10.148.1.195"
        private val PORT = 3001

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

        gridLayout {
            padding = dip(20)

            val mainContext = context

            verticalLayout {
                button("Play") {
                    onClick {
                        send(mainContext, "play")
                    }
                }.lparams(width = 500, height = 750)

                button("Skip") {
                    onClick {
                        send(mainContext, "skip")
                    }
                }.lparams(width = 500, height = 750)
            }

            verticalLayout {
                button("Pause") {
                    onClick {
                        send(mainContext, "pause")
                    }
                }.lparams(width = 500, height = 750)

                button("Back") {
                    onClick {
                        send(mainContext, "prev")
                    }
                }.lparams(width = 500, height = 750)
            }
        }
    }

}
